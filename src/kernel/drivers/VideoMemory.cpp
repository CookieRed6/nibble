#include <SFML/OpenGL.hpp>
#include <kernel/drivers/VideoMemory.hpp>
#include <kernel/drivers/GPU.hpp>
#include <iostream>
#include <cstring>

const uint64_t VideoMemory::nibblesPerPixel = 8;
const uint64_t VideoMemory::bytesPerPixel = 4;

// Vertex shader padrão do SFML sem alterações
const string VideoMemory::writeShaderVertex = R"(
void main()
{
    // transform the vertex position
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    // transform the texture coordinates
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

    // forward the vertex color
    gl_FrontColor = gl_Color;
}
)";

// Recebe uma textura com 1/8 do comprimento da tela
// esticada para o tamanho da tela e endereça cada 
// pixel na imagem como 8 pixels na tela.
// Para fazer isso considera cada canal como
// dois números de 4 bits
const string VideoMemory::writeShaderFragment = R"(
const float screen_w = 320.0;
// 4 canais na textura (RGBA)
const float bytes_per_pixel = 4.0;

// Textura "esticada"
uniform sampler2D texture;
// Textura 1xN da paleta
uniform sampler2D palette;

void main()
{
    // Coordenadas no espaço da textura
    vec2 coord = gl_TexCoord[0].xy;
    // Em qual nibble do pixel estamos
    int byte = int(mod(coord.x*screen_w, bytes_per_pixel));

    // Cor do pixel na textura
    vec4 pixel = texture2D(texture, coord);

    // Qual canal do pixel da textura vamos
    // usar para o pixel na tela
    float source;
    // Primeiro byte, segundo byte etc
    if (byte == 0)
        source = pixel.r;
    else if (byte == 1)
        source = pixel.g; 
    else if (byte == 2)
        source = pixel.b;
    else if (byte == 3)
        source = pixel.a;

    vec4 color = texture2D(palette, vec2(source, 0.5));

    // Não desenha pixels transparentes
    if (color.a == 0.0)
        discard;

    gl_FragColor = vec4(color.rgb, 1.0);
}
)";

VideoMemory::VideoMemory(sf::RenderWindow &window,
                         const unsigned int w,
                         const unsigned int h,
                         const uint64_t addr):
	w(w), h(h), address(addr), length(w*h), window(window), dirty(false),
    colormap(NULL) {
    // Tamanho da textura é 1/4 do tamanho da tela
    // uma vez que um pixel no sfml são quatro bytes
    // e no console é apenas um
	rwTex.create(w/bytesPerPixel, h);
	renderTex.create(w/bytesPerPixel, h);

	auto &tex = renderTex.getTexture();

	gpuSpr = sf::Sprite(tex);
	cpuSpr = sf::Sprite(rwTex);
    // Faz preencher toda a tela
    // e inverte o y
    gpuSpr.setScale(bytesPerPixel, -1);
    gpuSpr.setPosition(0, h);

    // Cria a textura da palleta
    paletteTex.create(GPU::paletteLength*GPU::paletteAmount, 1);

    // Tenta carregar o shader, em caso de erro termina
    // uma vez que não será possível mostrar nada
    if (!writeShader.loadFromMemory(writeShaderVertex, writeShaderFragment)) {
        cout << "video " << "error loading write shader" << endl;
        exit(1);
    }
    // Adiciona paleta e textura ao shader
    else {
        writeShader.setUniform("texture", tex);
        writeShader.setUniform("palette", paletteTex);
    }

    const uint64_t videoRamSize = w*h;

    // Limpa a memória de vídeo em RAM para 
    // deixar sincronizado com as texturas que estão limpas
    buffer = new uint8_t[videoRamSize];
    for (unsigned int i=0;i<videoRamSize;i++) {
            buffer[i] = (i>>8)%4;
    }

    // Inicializa com bytes não inicializados
    rwTex.update(buffer);
}

VideoMemory::~VideoMemory() {
    if (colormap != NULL) {
        stopCapturing();
    }

    delete buffer;
}

bool VideoMemory::startCapturing(const string& path) {
    int error;

    // Cria um colormap a partir da paleta
    colormap = getColorMap();

    // Abre um GIF pra salvar a tela
    gif = EGifOpenFileName(path.c_str(), false, &error);
    // Versão nova do GIF
    EGifSetGifVersion(gif, true);
    // Coonfigurações da screen
    error = EGifPutScreenDesc(gif, w, h,
                      GPU::paletteLength*GPU::paletteAmount, 0,
                      colormap);
    // Limpa a paleta que foi escrita
    GifFreeMapObject(colormap);

    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }

    char loop[] {
        0x01, 0x00, 0x00
    };
    error = 0;
    error |= EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE);
    error |= EGifPutExtensionBlock(gif, 0x0b, "NETSCAPE2.0");
    error |= EGifPutExtensionBlock(gif, 0x03, loop);
    error |= EGifPutExtensionTrailer(gif);

    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }

    return true;
}

bool VideoMemory::stopCapturing() {
    int error;
    EGifCloseFile(gif, &error);
    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }

    gif = NULL;
    colormap = NULL;

    return true;
}

bool VideoMemory::captureFrame() {
    int error;
    char graphics[] {
        0, 2&0xFF, 2>>8, 0
    };
    error = EGifPutExtension(
        gif,
        GRAPHICS_EXT_FUNC_CODE,
        sizeof(graphics),
        &graphics);

    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }

    error = EGifPutImageDesc(gif, 0, 0, w, h,
                             false, NULL);
    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }
    error = EGifPutLine(gif, buffer, w*h);
    if (error != GIF_OK) {
        cerr << GifErrorString(error) << endl;
        return false;
    }
}

ColorMapObject* VideoMemory::getColorMap() {
    // Tamanho da paleta do console
    uint64_t paletteSize = GPU::paletteLength*GPU::paletteAmount;
    // "Paleta" do GIF
    GifColorType colors[paletteSize];
    auto image = paletteTex.copyToImage();

    // Preenche o color map no formato do GIF
    // a partir do formato de paleta do console
    for (uint64_t i=0;i<paletteSize;i++) {
        sf::Color color = image.getPixel(i, 0);
        // Remove o alpha
        colors[i] = GifColorType {
            color.r, color.g, color.b
        };
    }

    colormap = GifMakeMapObject(paletteSize, colors);

    return colormap;
}

void VideoMemory::draw() {
    if (colormap == NULL) {
        startCapturing("screencap.gif");
    } else {
        captureFrame();
    }

    // Copia da nossa textura rw para a textura read-only do framebuffer
    // Utiliza BlendNone para copiar o alpha também
	renderTex.draw(cpuSpr, sf::BlendNone);
	// Desenha o framebuffer na tela, usando o shader para converter do
    // formato 1byte por pixel para cores RGBA nos pixels
    window.draw(gpuSpr, &writeShader);
}

void VideoMemory::updatePalette(const uint8_t* palette) {
    paletteTex.update(palette, paletteTex.getSize().x, paletteTex.getSize().y, 0, 0);
}

// Quantos bytes podemos transferir a partir do byte atual
uint64_t VideoMemory::nextTransferAmount(uint64_t current, uint64_t initial, uint64_t size) {
    // Terminamos
    if (current-initial >= size) {
        return 0;
    }

    // Se estamos no início de uma linha
    if (!(current%w)) {
        // e temos uma ou mais linhas para transferir, podemos transferi-las de uma vez
        uint64_t pixels = (size-(current-initial));
        if (pixels >= w) {
            return ((pixels/w)*w);
        }
        // se não, podemos transferir até o final dos dados
        else {
            return (size-(current-initial));
        }
    }
    // senão só podemos transferir até o final desta linha ou dos dados
    else {
        uint64_t pixels = (size-(current-initial));
        if (pixels >= w) {
            return w-(current%w);
        } else {
            return size-(current-initial);
        }
    }
}

uint64_t VideoMemory::bytesToTransferPixels(uint64_t bytes) {
    return bytes%bytesPerPixel == 0 ?
            bytes/bytesPerPixel : bytes/bytesPerPixel + 1;
}

uint64_t VideoMemory::bytesToPixels(uint64_t bytes) {
    return bytes/bytesPerPixel;
}

uint64_t VideoMemory::transferWidth(uint64_t pixels) {
    if (pixels > w/bytesPerPixel) {
        return w/bytesPerPixel;
    } else {
        return pixels;
    }
}

uint64_t VideoMemory::transferHeight(uint64_t pixels) {
    if (pixels/(w/bytesPerPixel) > 0) {
        return pixels/(w/bytesPerPixel);
    } else {
        return 1;
    }
}

// Escreve data (que tem size bytes) na posição p na memória de vídeo
uint64_t VideoMemory::write(const uint64_t p, const uint8_t* data, const uint64_t size) {
    // Copia dados para RAM
    memcpy(buffer+p, data, size);

    // Transfere para a GPU
    uint64_t transfered = 0;
    uint64_t toTransfer = 0;
    // Quantos bytes transferir
    while (toTransfer = nextTransferAmount(p+transfered, p, size)) {
        uint8_t *pixels;
        unsigned int x, y, offset;

        // Onde colocar
        x = bytesToPixels(p+transfered) % (w/bytesPerPixel);
        y = bytesToPixels(p+transfered) / (w/bytesPerPixel);
        // Inicia em qual byte dentro do pixel?
        offset = (p+transfered)%bytesPerPixel;

        // Alinha o ponteiro a margem do pixel,
        // Fazemos isso porque o OpenGL não
        // consegue atualizar apenas um byte.
        switch (offset) {
            case 0:
                pixels = buffer+p+transfered;
                break;
            case 1:
                pixels = buffer+p+transfered-1;
                break;
            case 2:
                pixels = buffer+p+transfered-2;
                break;
            case 3:
                pixels = buffer+p+transfered-3;
                break;
        }

        // Upload
        // Quantos pixels
        uint64_t transferPixels = bytesToTransferPixels(toTransfer);
        // Que equivalem a uma área de...
        unsigned int tw = transferWidth(transferPixels);
        unsigned int th = transferHeight(transferPixels);
        // Qual textura
        sf::Texture::bind(&rwTex);
        // Go go go
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        x, y,
                        tw, th,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels);
        // Remove a textura
        sf::Texture::bind(NULL);
        
        transfered += toTransfer;
    }

	return transfered;
}

uint64_t VideoMemory::read(const uint64_t p, uint8_t* data, const uint64_t size) {
    // Será necessário quando estivermos usando a GPU para desenhar
	//if (dirty) {
	//	// Carrega da GPU para a RAM
	//	img = tex->copyToImage();
	//	dirty = false;
	//}

    // Copia da memória de vídeo para o buffer do cliente
    memcpy(data, buffer+p, size);
	
	return size;
}

uint64_t VideoMemory::size() {
	return length;
}

uint64_t VideoMemory::addr() {
	return address;
}
