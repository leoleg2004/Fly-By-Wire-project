/********************************************************************************
 *
 * OdometryListener
 *
 * Copyright (c) 2026
 * All rights reserved.
 *
 * Davide Brugali, Università degli Studi di Bergamo
 *
 * -------------------------------------------------------------------------------
 * File: RoverKeyboard.hpp
 * Created: March 5, 2026
 * Author: <A HREF="mailto:brugali@unibg.it">Davide Brugali</A>
 * -------------------------------------------------------------------------------
 *
 * This software is published under a dual-license: GNU Lesser General Public
 * License LGPL 2.1 and BSD license. The dual-license implies that users of this
 * code may choose which terms they prefer.
 *
 * -------------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the name of the University of Bergamo nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version or the BSD license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL and the BSD license for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL and BSD license along with this program.
 *
 ******************************************************************************
 */
#ifndef STAR_ODOMETRY_LISTENER_H
#define STAR_ODOMETRY_LISTENER_H

#include "Listener.hpp"

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>

#include "geometry_msgsPubSubTypes.h"
#include "nav_msgsPubSubTypes.h"
#include <boost/thread/mutex.hpp>

class OdometryListener : public eprosima::fastdds::dds::DataReaderListener { 
public:
	OdometryListener() { }
	~OdometryListener() override {}
	void on_data_available(eprosima::fastdds::dds::DataReader* reader) override {
            SampleInfo info;  
            nav_msgs::msg::dds_::Odometry_ odo_msg;
            if (reader->take_next_sample(&odo_msg, &info) == ReturnCode_t::RETCODE_OK) {
                if (info.instance_state == ALIVE_INSTANCE_STATE) {
                    double qx, qy, qz, qw;
                    qx = odo_msg.pose().pose().orientation().x();
                    qy = odo_msg.pose().pose().orientation().y();
                    qz = odo_msg.pose().pose().orientation().z();
                    qw = odo_msg.pose().pose().orientation().w();
                    double siny_cosp = 2.0 * (qw * qz + qx * qy);
                    double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
        
                    mutex.lock();
                    odometry_px = odo_msg.pose().pose().position().x();
                    odometry_py = odo_msg.pose().pose().position().y();
                    odometry_oz = atan2(siny_cosp, cosy_cosp);
                    if(odometry_px < 0.00001)
                      odometry_px = 0.0;
                    if(odometry_py < 0.00001)
                      odometry_py = 0.0;
                    if(odometry_oz < 0.00001)
                      odometry_oz = 0.0;
                    mutex.unlock();
                }
            }
	}

	void on_subscription_matched(eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override {
	}

        void start(std::string topic, int domain_id) {
            // create the Listener
            std::cout << "\n\nStart the Odometry Listener" << std::endl;
            // start the listner
            eprosima::fastdds::dds::TypeSupport odom_type(new nav_msgs::msg::dds_::Odometry_PubSubType());
            listener = new Listener(this, &odom_type, "nav_msgs::msg::dds_::Odometry_");
            listener->start(topic, domain_id);
        }
	void stop() {
	    delete listener;
	}

        void get_odometry(double &odom_px, double &odom_py, double &odom_oz) {
            mutex.lock();
            odom_px = odometry_px; 
            odom_py = odometry_py;
            odom_oz = odometry_oz;
            mutex.unlock();
        }

private:
        std::string topic; 
        int domain_id;
        double odometry_px;
        double odometry_py;
        double odometry_oz;
	
	Listener *listener;
	boost::mutex mutex;
};

#endif
