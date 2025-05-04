#include "PushWork_.h"
#include <iostream>

int main() {
	PushWork pushWork;
	if (!pushWork.Init()) {
		std::cerr << "Failed to initialize PushWork" << std::endl;
		return -1;
	}

	pushWork.Start();
	std::cout << "PushWork started..." << std::endl << "Press Enter to stop..." << std::endl;
	std::cin.get();

	pushWork.Close();
	std::cout << "PushWork stopped." << std::endl;
	return 0;
}
