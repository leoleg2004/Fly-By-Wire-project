/*
 * GeometryBroadcastner.hpp
 */

#ifndef ERTS_DDS_CLIENT_H_
#define ERTS_DDS_CLIENT_H_


#include "Broadcastner.hpp"
#include "activity_library.h"


class Client {
public:
	Client();

	~Client() { }

	void start(std::string topic_name, int domain_id);
	
	void publish_request();

private:

	Broadcastner broadcastner;
    int counter = 0;

    pthread_t periodic_thread;
	t_activity_par activity_parameters;
};


void activity_function(void* instance) {
	Client* class_instance = (Client*) instance;
	class_instance->publish_request();
}

#endif
