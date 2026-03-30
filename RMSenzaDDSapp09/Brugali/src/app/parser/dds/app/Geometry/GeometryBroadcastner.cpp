/*
 * GeometryBroadcastner.cpp
 */

#include "GeometryBroadcastner.hpp"
#include <geometry_msgsPubSubTypes.hpp>

#define DOMAIN_ID 1

GeometryBroadcastner::GeometryBroadcastner() {
}

void GeometryBroadcastner::start(std::string topic_name, int domain_id) {
    eprosima::fastdds::dds::TypeSupport msg_type(new PointPubSubType());
    broadcastner.start(&msg_type, "Point", topic_name, domain_id);

	std::cout << "\nPress Enter to send a new message."<<std::endl;  
	std::cout << "Press Ctrl-C to stop the GeometryBroadcastner\n" << std::endl;

    Point point_msg;
    double counter = 0.0;
    while(1) {
    	std::cin.ignore();

    	counter += 0.1;
    	point_msg.x(1.0*counter);
    	point_msg.y(2.0*counter);
    	point_msg.z(3.0*counter);

    	std::cout << "GeometryBroadcastner send Point(" << point_msg.x() << ", " <<
    										 point_msg.y() << ", " <<
											 point_msg.z() << ")" << std::endl;

    	broadcastner.publish(&point_msg);
    }
}

int main(int argc, char** argv) {
    GeometryBroadcastner geometry;
    geometry.start("PointTopic", DOMAIN_ID);

    return 0;
}
