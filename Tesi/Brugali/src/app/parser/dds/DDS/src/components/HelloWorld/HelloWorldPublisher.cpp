/********************************************************************************
 *
 * HelloWorldPublisher
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
#include "HelloWorldPublisher.hpp"

#include <iostream>
#include <sstream>

#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>

int _kbhit() {
    static const int STDIN = 0;
    static bool initialized = false;

    if (! initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    if(bytesWaiting == 0)
    	return -1;
    //return bytesWaiting;
    return getchar()-48;
}

void HelloWorldPublisher::start() {
    std::cout << "HelloWorldPublisher::start()" << std::endl;

    // read DOMAIN_ID from the environment variable
    int domain_id = 111;
    const char* env_val = std::getenv("STAR_DOMAIN_ID");
    if (!env_val) {
        std::cerr << "Environment variable STAR_DOMAIN_ID is not set.\n";
        return;
    }
    try {
        domain_id = std::stoi(env_val);
    } catch (const std::exception& e) {
        std::cerr << "Invalid integer in STAR_DOMAIN_ID: " << env_val << "\n";
        return;
    }


    // init the activity parameters
    sprintf(activity_parameters.name,	"HelloWorldPublisher");
    activity_parameters.function = control_function;
    activity_parameters.period = 200;
    activity_parameters.print = false;
    activity_parameters.instance = this;

    // Create the periodic thread
    int ret_err = pthread_create( &periodic_thread, NULL, PeriodicTask, (void*) &activity_parameters);
    handle_error(ret_err, "Error in creating PeriodicTask");

    // start the listners
    odom_listener.start("rt/odom", domain_id);
    image_listener.start("rt/camera/image_color", domain_id);

    // start the broadcastner
    std::string topic_name = "rt/rover_twist";
    eprosima::fastdds::dds::TypeSupport twist_type(new geometry_msgs::msg::dds_::Twist_PubSubType());
    twist_broadcastner.start(&twist_type, "geometry_msgs::msg::dds_::Twist_", topic_name, domain_id);
    
    pthread_join( periodic_thread, NULL);
}

void HelloWorldPublisher::shutdown() {
}


void HelloWorldPublisher::compute_twist() { 
    /*
     * STEP 1: Get the odometry
     */
    double odom_px, odom_py, odom_oz;
    odom_listener.get_odometry(odom_px, odom_py, odom_oz);
    std::cout << "\nOdometry(" << odom_px << ", " << odom_py << ", " << (odom_oz*180.0/M_PI) << ")"<<std::endl;

    /*
     * STEP 2: Get the image
     */ 
    cv::Mat image;
    image_listener.get_image(image);
    std::cout << "Image w: " << image.cols << "  h: " << image.rows << std::endl;
	if(image.cols == 0 || image.rows == 0)
		return;
		  
    /*
  	 * STEP 3: Compute the centroid of the red path
  	 */  
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

    // Red wraps around the HSV hue range, so use two ranges
    cv::Mat mask1, mask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), mask2);
    redMask = mask1 | mask2;
    
    // clean up noise
    cv::erode(redMask, redMask, cv::Mat(), cv::Point(-1, -1), 1);
    cv::dilate(redMask, redMask, cv::Mat(), cv::Point(-1, -1), 2);
        
    // Compute moments of the red mask
    cv::Moments m = cv::moments(redMask, true);

    if (m.m00 == 0) {
        std::cout << "No red region detected.\n";
        return;
    }
    
    // Centroid coordinates
    int cx = static_cast<int>(m.m10 / m.m00);
    int cy = static_cast<int>(m.m01 / m.m00);

    std::cout << "Centroid of red region: (" << cx << ", " << cy << ")\n";

    // Draw centroid on the original image
    cv::circle(image, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);

    // Show results
    cv::namedWindow("Display Received Image", cv::WINDOW_AUTOSIZE );
    cv::waitKey(100);
    cv::imshow("Path centroid", image);
    
    
    /*
     * STEP 4: Read the twist from the keyboard
     */
    int key = 0;
    key = _kbhit();
    readKeyboard(key);

    
    /*
     * STEP 5: Publish the twist
     */
    std::cout << "Twist(" << twist_vx << ", " << twist_vy << ", " << (twist_wz*180.0/M_PI) << ")"<<std::endl;
    publish_twist(twist_vx, twist_vy, twist_wz);
}


void HelloWorldPublisher::publish_twist(double twist_vx, double twist_vy, double twist_wz) {
    geometry_msgs::msg::dds_::Twist_ twist_msg;
    twist_msg.linear().x(twist_vx);
    twist_msg.linear().y(twist_vy);
    twist_msg.angular().z(twist_wz);
    twist_broadcastner.publish(&twist_msg);	
}



void HelloWorldPublisher::readKeyboard(int key) {
	if(key==8){
		std::cout << " : X +\n";
		twist_vx += 0.1;
	} else if(key==2){
		std::cout << " : X -\n";
		twist_vx -= 0.1;
	} else if(key==4){
		std::cout << " : Y +\n";
		twist_vy += 0.1;
	} else if(key==6){
		std::cout << " : Y -\n";
		twist_vy -= 0.1;
	} else if(key==7){
		std::cout << " : Z +\n";
		twist_wz += 0.1;
	} else if(key==9){
		std::cout << " : Z -\n";
		twist_wz -= 0.1;
	} else if(key==0){
		std::cout << " : STOP\n";
		twist_vx = 0.0;
		twist_vy = 0.0;
		twist_wz = 0.0;
	}
	else {
		std::cout << "(8) X+  (2) X-  (7) Z+  (9) Z-  (0) STOP" << std::endl;
		return;
	}
}



int main(int argc, char *argv[]) {
    HelloWorldPublisher component;

    component.start();

    return 0;
}

