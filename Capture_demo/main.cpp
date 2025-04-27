#include "PushWork_.h"
#include <iostream>

int main() {
	PushWork pushWork;
	if (!pushWork.init()) {
		std::cerr << "Failed to initialize PushWork" << std::endl;
		return -1;
	}

	pushWork.start();
	std::cout << "PushWork started. Press Enter to stop..." << std::endl;
	std::cin.get();

	pushWork.stop();
	std::cout << "PushWork stopped." << std::endl;
	return 0;
}
