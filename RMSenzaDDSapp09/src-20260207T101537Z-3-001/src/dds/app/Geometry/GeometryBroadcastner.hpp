/*
 * GeometryBroadcastner.hpp
 */

#ifndef ERTS_DDS_GEOMETRY_BROADCASTNER_H_
#define ERTS_DDS_GEOMETRY_BROADCASTNER_H_


#include "Broadcastner.hpp"

class GeometryBroadcastner {
public:
	GeometryBroadcastner();

	~GeometryBroadcastner() { }

	void start(std::string topic_name, int domain_id);
	
private:

	Broadcastner broadcastner;
};

#endif
