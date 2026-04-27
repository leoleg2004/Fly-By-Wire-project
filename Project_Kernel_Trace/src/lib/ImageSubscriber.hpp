/********************************************************************************
 *
 * ImageSubscriber
 *
 * Copyright (c) 2019
 * All rights reserved.
 *
 * Davide Brugali, Università degli Studi di Bergamo
 *
 * -------------------------------------------------------------------------------
 * File: ImageSubscriber.hpp
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
#ifndef IMAGE_SUBSCRIBER_H
#define IMAGE_SUBSCRIBER_H

#include "ImageListener.hpp"
#include "activity_library.h"
#include "communication_library/Broadcastner.hpp"
#include "trace_marker.h"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>

#include "geometry_msgsPubSubTypes.h"
#include "nav_msgsPubSubTypes.h"

class ImageSubscriber {
public:
  ImageSubscriber() {}
  ~ImageSubscriber() {}

  void start();
  void shutdown();

private:
  ImageListener image_listener;

  pthread_t periodic_thread;
  t_activity_par activity_parameters;
};

int counter = 0;
void control_function(void *instance, int parameter) {
  char marker[64];

  snprintf(marker, sizeof(marker), "FUNCTION_START_receive_image");
  write_trace_marker(marker);

  std::cout << "---START_COMPUTATION" << std::endl;
  double result;
  for (int i = 1; i < 3 * 10000000; i++)
    result = atan2(sin(M_PI / 1000 * i), cos(M_PI / 100 * i));

  std::cout << "---END_COMPUTATION" << std::endl;

  snprintf(marker, sizeof(marker), "FUNCTION_END_receive_image");
  write_trace_marker(marker);
}

#endif
