/**
 *  LibUV.cpp
 * 
 *  Test program demonstrating AMQP send/receive functionality with LibUV
 * 
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2015 - 2017 Copernica BV
 *  @modified Enhanced with send/receive demo
 */

/**
 *  Dependencies
 */
#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <string>
#include <iostream>

/**
 *  Custom handler
 */
class MyHandler : public AMQP::LibUvHandler
{
private:
    /**
     *  Method called when a connection error occurs
     *  @param  connection
     *  @param  message
     */
    virtual void onError(AMQP::TcpConnection *connection, const char *message) override
    {
        std::cout << "Error: " << message << std::endl;
    }

    /**
     *  Method called when TCP connection is established
     *  @param  connection  The TCP connection
     */
    virtual void onConnected(AMQP::TcpConnection *connection) override 
    {
        std::cout << "Connected to RabbitMQ" << std::endl;
    }

public:
    /**
     *  Constructor
     *  @param  uv_loop
     */
    MyHandler(uv_loop_t *loop) : AMQP::LibUvHandler(loop) {}

    /**
     *  Destructor
     */
    virtual ~MyHandler() = default;
};

/**
 *  Main program
 *  @return int
 */
int main()
{
    // Access to the event loop
    auto *loop = uv_default_loop();
    
    // Handler for libuv
    MyHandler handler(loop);
    
    // Make a connection
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@localhost/"));
    
    // Create a channel
    AMQP::TcpChannel channel(&connection);
    
    // Declare an exchange
    channel.declareExchange("my_exchange", AMQP::direct)
        .onSuccess([]() {
            std::cout << "Exchange 'my_exchange' declared" << std::endl;
        })
        .onError([](const char* message) {
            std::cout << "Exchange declaration failed: " << message << std::endl;
        });

    // Declare a queue
    std::string queueName;
    channel.declareQueue(AMQP::exclusive)
        .onSuccess([&channel, &queueName](const std::string &name, uint32_t messagecount, uint32_t consumercount) {
            queueName = name;
            std::cout << "Declared queue: " << name << std::endl;

            // Bind queue to exchange
            channel.bindQueue("my_exchange", name, "my_routing_key")
                .onSuccess([]() {
                    std::cout << "Queue bound to exchange" << std::endl;
                });
        });

    // Set up consumer
    channel.consume(queueName, AMQP::noack)
        .onReceived([&channel](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
            std::string body(message.body(), message.bodySize());
            std::cout << "Received message: " << body << std::endl;
            
            // Optional: Publish a response message
            // channel.publish("my_exchange", "my_routing_key", "Response to: " + body);
        })
        .onSuccess([](const std::string &consumerTag) {
            std::cout << "Consumer started with tag: " << consumerTag << std::endl;
        })
        .onError([](const char *message) {
            std::cout << "Consume failed: " << message << std::endl;
        });

    // Publish some test messages after connection
    channel.onReady([&channel]() {
        for (int i = 0; i < 5; i++) {
            std::string message = "Test message " + std::to_string(i);
            channel.publish("my_exchange", "my_routing_key", message);
            std::cout << "Sent: " << message << std::endl;
        }
    });

    // Error handling for channel
    channel.onError([](const char* message) {
        std::cout << "Channel error: " << message << std::endl;
    });

    // Run the event loop
    std::cout << "Starting event loop..." << std::endl;
    uv_run(loop, UV_RUN_DEFAULT);

    // Cleanup
    uv_loop_close(loop);
    
    return 0;
}