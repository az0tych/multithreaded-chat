#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <locale>
#include <codecvt>
#include <thread>
#include <format>

#pragma comment(lib, "ws2_32.lib")
using namespace std;

// Преобразование строки в UTF-8
string to_utf8(const wstring& wstr) {
    wstring_convert<codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr);
}

// Преобразование строки из UTF-8
wstring from_utf8(const string& str) {
    wstring_convert<codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
}

// Функция для получения сообщений от сервера
void receiveMessages(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    while (true) {
        ZeroMemory(buffer, 1024);
        bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived > 0) {
            string receivedStr(buffer, 0, bytesReceived);
            wstring message = from_utf8(receivedStr);
            wcout << L"\n" << message << endl;
            wcout << L"Вы: " << flush;  // Возвращаем курсор для ввода
        }
    }
}

int main() {
    setlocale(LC_ALL, "Russian");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    
    string ip;
    int port;

    // Запрашиваем у пользователя IP и порт
    cout << "Введите IP адрес сервера: ";
    cin >> ip;
    cout << "Введите порт сервера: ";
    cin >> port;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // Подключение к серверу
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Ошибка подключения к серверу!" << endl;
        return 1;
    }

    cout << "Соединение с сервером установлено!\n";

    // Ввод юзернейма
    wstring username;
    while (true) {
        wcout << L"Введите ваш юзернейм: ";
        wcin >> username;
        if (!username.empty()) {
            break; // Выход из цикла, если введен ненулевой юзернейм
        }
        wcout << L"Юзернейм не может быть пустым. Попробуйте снова." << endl;
    }
    string utf8Username = to_utf8(username);
    send(clientSocket, utf8Username.c_str(), utf8Username.size() + 1, 0);

    // Запуск потока для получения сообщений от сервера
    thread receiveThread(receiveMessages, clientSocket);
    receiveThread.detach();

    // Отправка сообщений на сервер
    wstring message;
    while (true) {
        wcout << L"Вы: " << flush;
        getline(wcin, message);
        string utf8Message = to_utf8(message);
        send(clientSocket, utf8Message.c_str(), utf8Message.size() + 1, 0);
        if (message == L"BYE") {
            break;
        }
    }

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}