#include "TCPServer.h"

namespace SomeIP_Dispatcher {

std::vector<IPV4Address> TCPServer::getIPAddresses() {

	std::vector<IPV4Address> ipAddresses;

	struct ifaddrs* ifaddr, * ifa;
	int family, s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		log_error() << "Error getting local IP addresses";
	} else {
		/* Walk through linked list, maintaining head pointer so we can free list later */
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL)
				continue;

			family = ifa->ifa_addr->sa_family;

			if (family == AF_INET) {

				auto* p = reinterpret_cast<uint8_t*>(&( (struct sockaddr_in*) ifa->ifa_addr )->sin_addr);
				IPV4Address address(p);
				if ( !address.isLocalAddress() )
					ipAddresses.push_back(address);

				s =
					getnameinfo(ifa->ifa_addr,
						    (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
						    host, sizeof(host), nullptr, 0,
						    NI_NUMERICHOST);
				if (s != 0) {
					log_error() << "getnameinfo() failed: " << gai_strerror(s);
					throw new ConnectionExceptionWithErrno("getnameinfo() failed");
				}
			}
		}

		freeifaddrs(ifaddr);
	}

	return ipAddresses;
}

void TCPServer::init() {

	int tcpServerSocketHandle = 0;

	if ( ( tcpServerSocketHandle = ::socket(AF_INET, SOCK_STREAM, 0) ) < 0 ) {
		log_error("Failed to create socket");
		throw ConnectionExceptionWithErrno("Failed to create socket");
	}

	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(m_port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (true) {
		// we try to find an available TCP port to listen to
		for (int i = 0; i < 10; i++) {
			sin.sin_port = htons(m_port);
			if (::bind( tcpServerSocketHandle, (struct sockaddr*) &sin, sizeof(sin) ) != 0) {
				log_warn() << "Failed to bind TCP server socket " << m_port << ". Error : " << strerror(errno);
				m_port++;
			} else
				break;
		}
	} else if (::bind( tcpServerSocketHandle, (struct sockaddr*) &sin, sizeof(sin) ) != 0) {
		log_error( "Failed to bind TCP server socket %d. Error : %s", m_port, strerror(errno) );
		throw ConnectionExceptionWithErrno("Failed to bind TCP server socket");
	}

	if (::listen(tcpServerSocketHandle, SOMAXCONN) != 0) {
		log_error( "Failed to listen to TCP port %i. Error : %s", m_port, strerror(errno) );
		throw ConnectionExceptionWithErrno("Failed to listen to TCP port");
	}

	GIOChannel* tcpServerSocketChannel = g_io_channel_unix_new(tcpServerSocketHandle);

	if ( !g_io_add_watch(tcpServerSocketChannel, G_IO_IN | G_IO_HUP, onNewSocketConnection, this) ) {
		log_error("Cannot add watch on TCP GIOChannel!");
		throw ConnectionException("Failed to listen to TCP port");
	}

	log_info("TCP Server socket listening on port %d", m_port);

	setFileDescriptor(tcpServerSocketHandle);
}

}
