#include <chrono>
#include <iostream>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <string>
#include <vector>
#include <future>
#include <thread>

#pragma comment(lib,"Ws2_32.lib")

using namespace std;
using namespace chrono;
using namespace this_thread;

template<typename T>
vector<T> WaitForAll(vector<future<T>> & futures)
{
	vector<T> output;
	for (auto & future : futures) output.push_back(future.get());
	return output;
}

template<typename T>
int WaitForAny(vector<future<T>> & futures)
{
	for (auto index = 0; static_cast<unsigned int>(index) < futures.size(); )
	{
		if (!futures[index].valid()) continue;
		switch (futures[index].wait_for(seconds(0)))
		{
		case future_status::ready:
			return index;
		case future_status::deferred:
			++index;
			index %= futures.size();
			break;
		case future_status::timeout:
			++index;
			index %= futures.size();
			break;
		}
	}
	return 32768;
}

int main(const int argc, const char ** argv)
{
	if (argc != 2)
	{
		wcout << L"Usage: InternetConnectionSpeedIso [number of trials to run]\r\n";
		return -7;
	}
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData))
	{
		wcout << L"There was an issue initializing Winsock.\r\n";
		return -1;
	}
	vector<SOCKET> clients;
	for (auto index = 0; index < 4 * stoi(argv[1]); ++index)
	{
		auto client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!client)
		{
			wcout << L"There was an issue initializing the sockets.\r\n";
			for (auto innerIndex = 0; innerIndex < index; ++innerIndex) closesocket(clients[innerIndex]);
			WSACleanup();
			return -8;
		}
		clients.push_back(client);
	}
	struct addrinfo * result[4];
	if (getaddrinfo("www.google.com", "80", nullptr, &result[0]))
	{
		wcout << L"There was an issue resolving the first host name.\r\n";
		for (auto & client : clients) closesocket(client);
		WSACleanup();
		return -3;
	}
	if (getaddrinfo("www.microsoft.com", "80", nullptr, &result[1]))
	{
		wcout << L"There was an issue resolving the second host name.\r\n";
		for (auto & client : clients) closesocket(client);
		WSACleanup();
		return -4;
	}
	if (getaddrinfo("www.alibaba.com", "80", nullptr, &result[2]))
	{
		wcout << L"There was an issue resolving the third host name.\r\n";
		for (auto & client : clients) closesocket(client);
		WSACleanup();
		return -5;
	}
	if (getaddrinfo("www.apple.com", "80", nullptr, &result[3]))
	{
		wcout << L"There was an issue resolving the fourth host name.\r\n";
		for (auto & client : clients) closesocket(client);
		WSACleanup();
		return -6;
	}
	for (auto index = 0; index < 4 * stoi(argv[1]); ++index) if (connect(clients[index], result[index % 4]->ai_addr, result[index % 4]->ai_addrlen))
	{
		wcout << L"There was an issue connecting each of the clients to their respective test endpoints.\r\n";
		for (auto & client : clients) closesocket(client);
		WSACleanup();
		return -9;
	}
	auto request = "GET / HTTP/1.1";
	vector<future<duration<double,milli>>> futures;
	futures.push_back(async([] {sleep_for(milliseconds(5000)); return duration<double, milli>{0}; }));
	for (auto index = 0; index < stoi(argv[1]); ++index) futures.push_back(async([clients, index, request]
	{
		vector<future<duration<double, milli>>> tests;
		for (auto innerIndex = 0; innerIndex < 4; ++innerIndex) tests.push_back(async([clients, index, innerIndex, request] 
		{
			auto bitsLeftToSend = sizeof(request);
			auto bitsSent = 0;
			while(bitsLeftToSend)
			{
				auto packetSize = send(clients[index + innerIndex], &request[bitsSent], sizeof(request), 0);
				bitsSent += packetSize;
				bitsLeftToSend -= packetSize;
			}
			send(clients[index + innerIndex], request, sizeof(request), 0);
			auto start = steady_clock::now();
			char signal;
			recv(clients[index + innerIndex], &signal, 1, 0);
			return duration<double, milli> { duration_cast<milliseconds>(steady_clock::now() - start) };
		}));
		auto results = WaitForAll(tests);
		auto sum = static_cast<double>(0);
		for (auto& resultingTime : results) sum += resultingTime.count();
		return duration<double, milli> {sum / 4.0};
	}));
	while(true)
	{
		auto number = WaitForAny(futures);
		if (number == 0 || futures.size() < 2) break;
		wcout << L"Test: " << futures[number].get().count() << L" ms\r\n";
		futures.erase(futures.begin() + number);
	}
	for (auto & client : clients) closesocket(client);
	WSACleanup();
	return 0;
}