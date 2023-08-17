#include <iostream>
#include "Client.h"

using namespace std;

int main(int argc, char** argv) {

	Client cl;
	cl.Connect();

	while (true) {
		string msg;
		cin >> msg;
		cl.Send(msg);
	}

	return 0;
}