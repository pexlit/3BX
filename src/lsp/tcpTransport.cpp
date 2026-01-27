#include "tcpTransport.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace lsp {

// TcpTransport implementation

TcpTransport::TcpTransport(int socket) : socket(socket) {}

TcpTransport::~TcpTransport() { close(); }

ssize_t TcpTransport::read(char *buffer, size_t count) {
	if (socket < 0)
		return -1;
	return recv(socket, buffer, count, 0);
}

ssize_t TcpTransport::write(const char *buffer, size_t count) {
	if (socket < 0)
		return -1;
	return send(socket, buffer, count, 0);
}

bool TcpTransport::isConnected() const { return socket >= 0; }

void TcpTransport::close() {
	if (socket >= 0) {
		::close(socket);
		socket = -1;
	}
}

// TcpServer implementation

TcpServer::TcpServer(int port) : port(port) {}

TcpServer::~TcpServer() { shutdown(); }

bool TcpServer::setup() {
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket < 0) {
		std::cerr << "[LSP ERROR] Failed to create socket: " << strerror(errno) << std::endl;
		return false;
	}

	// Allow socket reuse
	int opt = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		std::cerr << "[LSP ERROR] Failed to set socket options: " << strerror(errno) << std::endl;
		return false;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
		std::cerr << "[LSP ERROR] Failed to bind socket: " << strerror(errno) << std::endl;
		return false;
	}

	if (listen(serverSocket, 1) < 0) {
		std::cerr << "[LSP ERROR] Failed to listen on socket: " << strerror(errno) << std::endl;
		return false;
	}

	return true;
}

std::unique_ptr<TcpTransport> TcpServer::acceptConnection() {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);
	if (clientSocket < 0) {
		return nullptr;
	}

	return std::make_unique<TcpTransport>(clientSocket);
}

void TcpServer::shutdown() {
	if (serverSocket >= 0) {
		::close(serverSocket);
		serverSocket = -1;
	}
}

} // namespace lsp
