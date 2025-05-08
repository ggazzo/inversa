#include <Arduino.h>
#include <ESPAsyncWebServer.h>


class WebSocketBroadcastPrint : public Print {
private:
    AsyncWebSocket* webSocket;
    std::function<void(uint8_t* data, size_t len)> onMessageCallback;

public:
    WebSocketBroadcastPrint(AsyncWebSocket* ws) : webSocket(ws) {
         webSocket->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                  AwsEventType type, void* arg, uint8_t* data, size_t len) {
            handleWebSocketEvent(server, client, type, data, len);
        });
    }

    
    size_t write(uint8_t character) override {
        char buffer[2] = {character, '\0'};
        webSocket->textAll(buffer);
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        webSocket->textAll(reinterpret_cast<const char*>(buffer), size);
        return size;
    }

     // Método para configurar o callback de mensagens
    void onMessage(std::function<void(uint8_t* data, size_t len)> callback) {
        onMessageCallback = callback;
    }

    // Gerenciamento de eventos do WebSocket
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                              AwsEventType type, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("Cliente %u conectado\n", client->id());  
                break;
            case WS_EVT_DISCONNECT:
                Serial.printf("Cliente %u desconectado\n", client->id());
                break;
            case WS_EVT_DATA:
                if (onMessageCallback) {
                    onMessageCallback(data, len); // Chama o callback configurado
                }
                break;
            case WS_EVT_PONG:
            case WS_EVT_ERROR:
                // Tratar eventos de PONG ou ERRO se necessário
                break;
        }
    }
};
