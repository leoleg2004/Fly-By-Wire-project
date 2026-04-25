/********************************************************************************
 *
 * ImageSender
 *
 * Copyright (c) 2019
 * All rights reserved.
 *
 * Davide Brugali, Università degli Studi di Bergamo
 *
 * -------------------------------------------------------------------------------
 * File: IndoorPositionSensor.cpp
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
#include "ImageSender.hpp"

#include <iostream>
#include <sstream>

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>

void ImageSender::start() {
  std::cout << "ImageSender::start()" << std::endl;

  int domain_id = 111;

  // init the activity parameters
  sprintf(activity_parameters.name, "ImageSender");
  activity_parameters.function = control_function;
  activity_parameters.period = 200;
  activity_parameters.print = false;
  activity_parameters.instance = this;

  // Create the periodic thread
  int ret_err = pthread_create(&periodic_thread, NULL, PeriodicTask,
                               (void *)&activity_parameters);
  handle_error(ret_err, "Error in creating PeriodicTask");

  // start the broadcastner
  std::string topic_name = "rt/moon";
  eprosima::fastdds::dds::TypeSupport image_type(
      new sensor_msgs::msg::dds_::Image_PubSubType());
  image_broadcastner.start(&image_type, "sensor_msgs::msg::dds_::Image_",
                           topic_name, domain_id);

  pthread_join(periodic_thread, NULL);
}

void ImageSender::shutdown() {}

void ImageSender::send_image() {
  counter++;
  if (counter == 4)
    counter = 1;
  std::cout << "\n\n*** send_image " << counter << " ***" << std::endl;

  std::string file_name = "moon_";
  file_name.append(std::to_string(counter));
  file_name.append(".png");
  cv::Mat image = cv::imread(file_name, cv::IMREAD_COLOR);

  if (!image.data) {
    std::cout << "Unable to open file: " << file_name << std::endl;
    return;
  }

  sensor_msgs::msg::dds_::Image_ msg;

  msg.height(image.rows);
  msg.width(image.cols);
  msg.encoding(cvTypeToEncoding(image.type()));

  std::cout << "Image encoding : " << msg.encoding() << std::endl;
  msg.is_bigendian(false); // most systems are little-endian
  msg.step(image.step);

  size_t size = image.step * image.rows;
  msg.data().resize(size);

  if (image.isContinuous()) {
    memcpy(msg.data().data(), image.data, size);
  } else {
    // Handle non-contiguous matrices safely
    for (int y = 0; y < image.rows; ++y) {
      memcpy(&msg.data()[y * msg.step()], image.ptr(y), image.step);
    }
  }

  std::cout << "Image w: " << image.cols << "  h: " << image.rows << std::endl;

  image_broadcastner.publish(&msg);
}

int main(int argc, char *argv[]) {
  init_tracing();

  ImageSender component;
  component.start();

  close_tracing();
  return 0;
}
