/********************************************************************************
 *
 * HelloWorldPublisher
 *
 * Copyright (c) 2026
 * All rights reserved.
 *
 * Davide Brugali, Università degli Studi di Bergamo
 *
 * -------------------------------------------------------------------------------
 * File: HelloWorldPublisher.hpp
 * Created: May 5, 2019
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
#ifndef HELLO_WORLD_PUBLISHER_H
#define HELLO_WORLD_PUBLISHER_H

#include "Broadcastner.hpp"
#include "OdometryListener.hpp"
#include "ImageListener.hpp"
#include "activity_library.h"

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


class HelloWorldPublisher { 
public:
	HelloWorldPublisher() {
	    twist_vx = 0.0;
	    twist_vy = 0.0;
	    twist_wz = 0.0;
	}
	~HelloWorldPublisher() { }

        void start();
	void shutdown();

        void compute_twist();
	
private:
	Broadcastner twist_broadcastner;
	OdometryListener odom_listener;
	ImageListener image_listener;
	
    pthread_t periodic_thread;
	t_activity_par activity_parameters;
        
    double twist_vx, twist_vy, twist_wz;
	void publish_twist(double twist_vx, double twist_vy, double twist_wz);
	
	void readKeyboard(int key);
	
};

int counter = 0;
void control_function(void* instance) {
    HelloWorldPublisher* class_instance = (HelloWorldPublisher*) instance;
    
    counter++;
    std::cout << "\n\n*** control_function " << counter << " ***" << std::endl;
    class_instance->compute_twist();
}

#endif
