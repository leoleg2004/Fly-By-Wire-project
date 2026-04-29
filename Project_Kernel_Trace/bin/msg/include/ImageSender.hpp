/*********************************************************************************
 *
 * ImageSender
 *
 * Copyright (c) 2019
 * All rights reserved.
 *
 * Davide Brugali, Università degli Studi di Bergamo
 *
 * -------------------------------------------------------------------------------
 * File: ImageSender.hpp
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
#ifndef IMAGE_SENDER_H
#define IMAGE_SENDER_H

#include "activity_library.h"
#include "Broadcastner.hpp"
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
#include "sensor_msgsPubSubTypes.h"

#include <opencv2/opencv.hpp>

class ImageSender {
public:
  ImageSender() {}
  ~ImageSender() {}

  void start();
  void shutdown();

  void send_image();

private:
  Broadcastner image_broadcastner;

  pthread_t periodic_thread;
  t_activity_par activity_parameters;

  int counter = 0;

  std::string cvTypeToEncoding(int type) {
    switch (type) {
    case CV_8UC1:
      return "mono8";
    case CV_8UC2:
      return "8UC2";
    case CV_8UC3:
      return "bgr8";
    case CV_8UC4:
      return "bgra8";

    case CV_16UC1:
      return "mono16";
    case CV_16UC2:
      return "16UC2";
    case CV_16UC3:
      return "16UC3";
    case CV_16UC4:
      return "16UC4";

    case CV_32FC1:
      return "32FC1";
    case CV_32FC2:
      return "32FC2";
    case CV_32FC3:
      return "32FC3";
    case CV_32FC4:
      return "32FC4";

    default:
      throw std::runtime_error("Unsupported cv::Mat type");
    }
  }
};

inline void control_function(void *instance, int parameter) {
  ImageSender *class_instance = (ImageSender *)instance;
  char marker[64];

  snprintf(marker, sizeof(marker), "FUNCTION_START_send_image");
  write_trace_marker(marker);

  class_instance->send_image();

  snprintf(marker, sizeof(marker), "FUNCTION_END_send_image");
  write_trace_marker(marker);
}

#endif
