#ifndef VIDEO_MEMORY_H
#define VIDEO_MEMORY_H

#include <cstdint>
#include <kernel/Memory.hpp>
#include <SFML/Graphics.hpp>
#include <gif_lib.h>

using namespace std;

class VideoMemory : public Memory {
    // Permite o acesso as fun��e protected para a classe
    // GPUMemory
    friend class GPUMemory;
    // Detalhes da mem�ria
	const uint64_t length;
	const uint64_t address;
	const unsigned int w, h;
	// Textura contendo a imagem que � vis�vel na tela,
	// funciona como mem�ria de v�deo
	sf::RenderTexture gpuRenderTexture;
	// �reas para desenho da gpuRenderTexture e cpuTexture
	sf::Sprite gpuSpr, cpuSpr;
    // Texturas para guardar os timings dos desenhos feitos pela cpu
    // e pela gpu, de forma que eles possam ser combinados na ordem
    // correta, facilitando para o desenvolvedor
    sf::RenderTexture gpuRenderTiming;
    sf::Texture cpuTiming;
    uint8_t *timingBuffer;
    // Vertex arrays utilizadas para desenhar informa��o de timing
    vector<sf::VertexArray> gpuTimingArrays;
    uint64_t gpuTimingCount;
    uint64_t cpuTimingCount;
    // Contador de draws
    uint64_t currentDraw;
    // Textura que permite a leitura e escrita.
    // Mem�ria de v�deo para opera��es n�o aceleradas em hardware
	sf::Texture cpuTexture;
	// Vers�o do framebuffer na RAM da CPU. O booleano dirty indica
	// quando a vers�o da GPU precisa ser carregada, mas ela s� � carregada
	// para essa image quando alguma opera��o de read precisa ser feita.
	sf::Image img;
	bool dirty;
    uint8_t *buffer;
	// Refer�ncia para a janela para que possamos desenhar para ela
	sf::RenderWindow &window;
    // C�digo e o shader utilizado para desenhar em write()s
    const static string writeShaderVertex;
    const static string writeShaderFragment;
    sf::Shader writeShader;
    // Textura utilizada como paleta pelo shader
    sf::Texture paletteTex;
    // Arquivo para salvar gifs
    GifFileType *gif;
    // Paleta do gif
    ColorMapObject *colormap;
public:
    const static uint64_t nibblesPerPixel;
    const static uint64_t bytesPerPixel;
    const static uint32_t vertexArrayLength;
public:
	VideoMemory(sf::RenderWindow&,
                const unsigned int,
                const unsigned int,
                const uint64_t);
	~VideoMemory();

    // Fecha arquivos abertos
    void close();

	void draw();

	uint64_t write(const uint64_t, const uint8_t*, const uint64_t);
	uint64_t read(const uint64_t, uint8_t*, const uint64_t);

	uint64_t size();
	uint64_t addr();

    // Chamado por paletteMemory quando o usu�rio troca a paleta
    void updatePalette(const uint8_t*);
protected:
    // Opera��es nas VertexArrays utilizadas para
    // desenho na GPU
    void drawGpuTiming(uint64_t,
                       uint32_t, uint32_t,
                       uint32_t, uint32_t);
    void execGpuCommand(uint8_t*);
private:
    void drawCpuTiming(uint32_t, uint64_t, uint64_t);
    void clearCpuTiming();
    // GIFs
    bool startCapturing(const string&);
    bool captureFrame();
    bool stopCapturing();
    ColorMapObject* getColorMap();
};

#endif /* VIDEO_MEMORY_H */
