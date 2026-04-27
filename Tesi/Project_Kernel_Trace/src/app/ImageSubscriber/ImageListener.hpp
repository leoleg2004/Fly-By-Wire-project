/********************************************************************************
 *
 * ImageListener
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
#ifndef STAR_IMAGE_LISTENER_H
#define STAR_IMAGE_LISTENER_H

#include "../../lib/communication_library/Listener.hpp"

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
#include "sensor_msgsPubSubTypes.h"

#include <boost/thread/mutex.hpp>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>
#include <memory>


class ImageListener
 : public eprosima::fastdds::dds::DataReaderListener { 
public:
	ImageListener() { }
	~ImageListener() override {}
	void on_data_available(eprosima::fastdds::dds::DataReader* reader) override {
// LABEL: START_DDS_DATA_AVAILABLE
		std::cout << "\n        START_DDS_DATA_AVAILABLE" << std::endl;
        SampleInfo info;  
        sensor_msgs::msg::dds_::Image_ image_msg;
        if (reader->take_next_sample(&image_msg, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.instance_state == ALIVE_INSTANCE_STATE) {
                image = imageToCv(image_msg, "bgr8");
		        if(! image.empty()) {
			    	cv::namedWindow("Received Image", cv::WINDOW_AUTOSIZE );
				    cv::waitKey(100);
				    cv::imshow("Moon", image);
				}
            }
        }
		std::cout << "        END_DDS_DATA_AVAILABLE\n" << std::endl;
// LABEL: END_DDS_DATA_AVAILABLE
	}

	void on_subscription_matched(eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override {
	}

    void start(std::string topic, int domain_id) {
        // create the Listener
        std::cout << "\n\nStart the Image Listener" << std::endl;
        // start the listner
        eprosima::fastdds::dds::TypeSupport image_type(new sensor_msgs::msg::dds_::Image_PubSubType());
        listener = new Listener(this, &image_type, "sensor_msgs::msg::dds_::Image_");
        listener->start(topic, domain_id);
    }
        
	void stop() {
	    delete listener;
	}



private:
        std::string topic; 
        int domain_id;
	cv::Mat image;
	
	Listener *listener;
	boost::mutex mutex;
		
	int encodingToCvType(const std::string& encoding) {
          if (encoding == "mono8")   return CV_8UC1;
          if (encoding == "mono16")  return CV_16UC1;

          if (encoding == "bgr8")    return CV_8UC3;
          if (encoding == "rgb8")    return CV_8UC3;

          if (encoding == "bgra8")   return CV_8UC4;
          if (encoding == "rgba8")   return CV_8UC4;

          if (encoding == "bgr16")   return CV_16UC3;
          if (encoding == "rgb16")   return CV_16UC3;

          if (encoding == "32FC1")   return CV_32FC1;
          if (encoding == "32FC2")   return CV_32FC2;
          if (encoding == "32FC3")   return CV_32FC3;
          if (encoding == "32FC4")   return CV_32FC4;

          if (encoding == "16UC1")   return CV_16UC1;
          if (encoding == "16UC3")   return CV_16UC3;

          if (encoding == "8UC1")    return CV_8UC1;
          if (encoding == "8UC2")    return CV_8UC2;
          if (encoding == "8UC3")    return CV_8UC3;
          if (encoding == "8UC4")    return CV_8UC4;

          throw std::runtime_error("Unsupported encoding: " + encoding);
      }

      cv::Mat imageToCvMat(const sensor_msgs::msg::dds_::Image_& msg, bool copy = false) {
          int type = encodingToCvType(msg.encoding());

          cv::Mat mat(
            msg.height(),
            msg.width(),
            type,
            const_cast<unsigned char*>(msg.data().data()),
            msg.step()
          );

          if (copy)
              return mat.clone();  // deep copy

        return mat; // zero-copy
    }
    
    cv::Mat convertToEncoding(const cv::Mat& input,
                          const std::string& src_encoding,
                          const std::string& dst_encoding) {
        if (src_encoding == dst_encoding)
            return input.clone();

        cv::Mat output;

        if (src_encoding == "rgb8" && dst_encoding == "bgr8")
           cv::cvtColor(input, output, cv::COLOR_RGB2BGR);

        else if (src_encoding == "bgr8" && dst_encoding == "rgb8")
            cv::cvtColor(input, output, cv::COLOR_BGR2RGB);

        else if (src_encoding == "bgra8" && dst_encoding == "bgr8")
            cv::cvtColor(input, output, cv::COLOR_BGRA2BGR);

        else if (src_encoding == "rgba8" && dst_encoding == "bgr8")
            cv::cvtColor(input, output, cv::COLOR_RGBA2BGR);

        else if (src_encoding == "mono8" && dst_encoding == "bgr8")
            cv::cvtColor(input, output, cv::COLOR_GRAY2BGR);

        else if (src_encoding == "mono8" && dst_encoding == "rgb8")
            cv::cvtColor(input, output, cv::COLOR_GRAY2RGB);

        else
            throw std::runtime_error(
                "Unsupported conversion: " + src_encoding + " -> " + dst_encoding
            );

        return output;
    }

    cv::Mat imageToCv(const sensor_msgs::msg::dds_::Image_& msg,
                     const std::string& desired_encoding = "",
                     bool copy = false) {
        cv::Mat mat = imageToCvMat(msg, copy);

        if (!desired_encoding.empty() && desired_encoding != msg.encoding()) {
            return convertToEncoding(mat, msg.encoding(), desired_encoding);
        }

        return mat;
    }

    /*
     * From cv::Mat to sensor_msgs::msg::dds_::Image_
     */

    std::string cvTypeToEncoding(int type) {
        switch (type) {
            case CV_8UC1:  return "mono8";
            case CV_8UC2:  return "8UC2";
            case CV_8UC3:  return "bgr8";
            case CV_8UC4:  return "bgra8";

            case CV_16UC1: return "mono16";
            case CV_16UC2: return "16UC2";
            case CV_16UC3: return "16UC3";
            case CV_16UC4: return "16UC4";

            case CV_32FC1: return "32FC1";
            case CV_32FC2: return "32FC2";
            case CV_32FC3: return "32FC3";
            case CV_32FC4: return "32FC4";

            default:
              throw std::runtime_error("Unsupported cv::Mat type");
        }
    }


    sensor_msgs::msg::dds_::Image_ cvMatToImage(const cv::Mat& image) {
        sensor_msgs::msg::dds_::Image_ msg;

        msg.height(image.rows);
        msg.width(image.cols);
        msg.encoding(cvTypeToEncoding(image.type()));
        msg.is_bigendian(false);  // most systems are little-endian
        msg.step(image.step);

        size_t size = image.step * image.rows;
        msg.data().resize(size);

        if (image.isContinuous()) {
            memcpy(msg.data().data(), image.data, size);
        }
        else {
            // Handle non-contiguous matrices safely
            for (int y = 0; y < image.rows; ++y) {
                memcpy(
                    &msg.data()[y * msg.step()],
                    image.ptr(y),
                    image.step
                );
            }
        }

        return msg;
    }

};

#endif
