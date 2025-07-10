#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VkEngine::Engine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
