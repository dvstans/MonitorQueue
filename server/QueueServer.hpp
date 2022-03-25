#include "Queue.hpp"

class QueueServer {
public:
    QueueServer();
    ~QueueServer();

    void listen();
private:
    Queue   m_queue;
};
