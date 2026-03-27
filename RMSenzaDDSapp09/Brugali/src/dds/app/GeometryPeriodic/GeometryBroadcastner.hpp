/*
 * GeometryBroadcastner.hpp
 */

#ifndef ERTS_DDS_GEOMETRY_BROADCASTNER_H_
#define ERTS_DDS_GEOMETRY_BROADCASTNER_H_


#include "Broadcastner.hpp"
#include "activity_library.h"


class GeometryBroadcastner {
public:
	GeometryBroadcastner();

	~GeometryBroadcastner() { }

	void start(std::string topic_name, int domain_id);
	
	void publish_point();

private:

	Broadcastner broadcastner;
    double counter = 0.0;

    pthread_t periodic_thread;
	t_activity_par activity_parameters;
};


void activity_function(void* instance) {
	GeometryBroadcastner* class_instance = (GeometryBroadcastner*) instance;
	class_instance->publish_point();
}

#endif
