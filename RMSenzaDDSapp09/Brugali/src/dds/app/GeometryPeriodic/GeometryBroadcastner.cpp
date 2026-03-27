/*
 * GeometryBroadcastner.cpp
 */

#include "GeometryBroadcastner.hpp"

#include <geometry_msgsPubSubTypes.h>

GeometryBroadcastner::GeometryBroadcastner() {
}

void GeometryBroadcastner::start(std::string topic_name, int domain_id) {
    eprosima::fastdds::dds::TypeSupport msg_type(new PointPubSubType());
    broadcastner.start(&msg_type, "Point", topic_name, domain_id);

    std::cout << "\nPress Ctrl-C to stop the GeometryBroadcastner" << std::endl;

	/* init the activity parameters */
	sprintf(activity_parameters.name,	"GeometryBroadcastner");
	activity_parameters.function = activity_function;
	activity_parameters.period = 800;
	activity_parameters.instance = this;

	/* Create the periodic thread */
    int ret_err = pthread_create( &periodic_thread, NULL, PeriodicTask, (void*) &activity_parameters);
    handle_error(ret_err, "Error in creating PeriodicTask 1");

    pthread_join( periodic_thread, NULL);
}


void GeometryBroadcastner::publish_point() {
    Point point_msg;

	counter += 0.1;
	point_msg.x(1.0*counter);
	point_msg.y(2.0*counter);
	point_msg.z(3.0*counter);

	std::cout << "GeometryBroadcastner send Point(" << point_msg.x() << ", " <<
										 point_msg.y() << ", " <<
										 point_msg.z() << ")" << std::endl;
	broadcastner.publish(&point_msg);
}



int main(int argc, char** argv) {
    GeometryBroadcastner geometry;
    geometry.start("PointTopic", 1);

    return 0;
}
