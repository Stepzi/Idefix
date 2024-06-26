/*  
 * 
 * Copyright © 2024 DTU,
 * Author:
 * Christian Andersen jcan@dtu.dk
 * 
 * The MIT License (MIT)  https://mit-license.org/
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the “Software”), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, 
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software 
 * is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies 
 * or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE. */

/**
 * ArUco specific code is from
 * https://docs.opencv.org/3.4.20/d5/dae/tutorial_aruco_detection.html
 * */

#include <string>
#include <string.h>
#include <math.h>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>

#include "mgolfball.h"
#include "uservice.h"
#include "scam.h"

// create value
Mgolfball golfball;
namespace fs = std::filesystem;

int c_lb1, c_lb2, c_lb3, c_ub1, c_ub2, c_ub3;
int hough_minDist,hough_p1,hough_p2,minRad,maxRad;

void Mgolfball::setup()
{ // ensure there is default values in ini-file
  if (not ini.has("golfball"))
  { // no data yet, so generate some default values
    ini["golfball"]["imagepath"] = "golfball";
    ini["golfball"]["save"] = "false";
    ini["golfball"]["log"] = "true";
    ini["golfball"]["print"] = "true";
    ini["golfball"]["hough_minDist"] = "100";
    ini["golfball"]["hough_p1"] = "200";
    ini["golfball"]["hough_p2"] = "100";
    ini["golfball"]["minRad"] = "40";
    ini["golfball"]["maxRad"] = "60";
    ini["golfball"]["color_lb"] = "10 100 100";
    ini["golfball"]["color_ub"] = "20 255 255";
  }
  // get values from ini-file
  fs::create_directory(ini["golfball"]["imagepath"]);
  //
  debugSave = ini["golfball"]["save"] == "true";
  toConsole = ini["golfball"]["print"] == "true";
  hough_minDist = strtol(ini["golfball"]["hough_minDist"].c_str(), nullptr, 10);
  hough_p1 = strtol(ini["golfball"]["hough_p1"].c_str(), nullptr, 10);
  hough_p2 = strtol(ini["golfball"]["hough_p2"].c_str(), nullptr, 10);
  minRad = strtol(ini["golfball"]["minRad"].c_str(), nullptr, 10);
  minRad = (minRad > 0) ? minRad : 1;
  maxRad = strtol(ini["golfball"]["maxRad"].c_str(), nullptr, 10);
  maxRad = (maxRad > 0) ? maxRad : 1000;

  const char * p1 = ini["golfball"]["color_lb"].c_str();
  c_lb1 = strtol(p1, (char**)&p1,10);
  c_lb2 = strtol(p1, (char**)&p1,10);
  c_lb3 = strtol(p1, (char**)&p1,10);

  p1 = ini["golfball"]["color_ub"].c_str();
  c_ub1 = strtol(p1, (char**)&p1,10);
  c_ub2 = strtol(p1, (char**)&p1,10);
  c_ub3 = strtol(p1, (char**)&p1,10);


  //
  if (ini["golfball"]["log"] == "true")
  { // open logfile
    std::string fn = service.logPath + "log_golfball.txt";
    logfile = fopen(fn.c_str(), "w");
    fprintf(logfile, "%% Vision activity (%s)\n", fn.c_str());
    fprintf(logfile, "%% 1 \tTime (sec)\n");
    fprintf(logfile, "%% 2 \tDetected marker in this image\n");
    fprintf(logfile, "%% 3 \tDetected marker code\n");
    fprintf(logfile, "%% 4 \tMarker size (position in same units as size)\n");
    fprintf(logfile, "%% 5,6,7 \tDetected marker position in camera coordinates (x=right, y=down, z=forward)\n");
    fprintf(logfile, "%% 8,9,10 \tDetected marker orientation in Rodrigues notation (vector, rotated)\n");
  }
}


void Mgolfball::terminate()
{ // wait for thread to finish
  if (logfile != nullptr)
  {
    fclose(logfile);
    logfile = nullptr;
  }
}

void Mgolfball::toLog(const char * message)
{
  if (not service.stop)
  {
    if (logfile != nullptr)
    { // log_pose
      fprintf(logfile, "%lu.%04ld %s\n", imgTime.getSec(), imgTime.getMicrosec()/100, message);
    }
    if (toConsole)
    { // print_pose
      printf("%lu.%04ld %s\n", imgTime.getSec(), imgTime.getMicrosec()/100, message);
    }
  }
}

bool Mgolfball::findGolfball(std::vector<int>& pos, std::vector<cv::Point> roi, cv::Mat *sourcePtr,float density_thr, int arg_minRad, int arg_maxRad)
{ // taken from https://docs.opencv.org

  // toLog("start find golfball");
  // Get frame 
  cv::Mat frame;
  if (sourcePtr == nullptr)
  {
    frame = cam.getFrameRaw();
    imgTime = cam.imgTime;
  }
  else
  {
    frame = *sourcePtr;
  }
  //
  if (frame.empty())
  {
    printf("MVision::findGolfball: Failed to get an image\n");
    return 0;
  }


  
  // toLog("got frame");
  // cv::Mat img;
  // if (debugSave)
  //   frame.copyTo(img);
  //=============================================

  // filter
  // blur
  // convert colors
  // apply color filter
  // detect contours
  // count == no. of detected golfball candidates
  // set bool to true if no of contours greater than 0
  // choose closest

// Create a mask
  cv::Mat ROI_mask = cv::Mat::zeros(frame.size(), CV_8UC1);
  // std::vector<std::vector<cv::Point>> contour_vec;
  // contour_vec.push_back(roi);
  cv::fillConvexPoly(ROI_mask, roi, cv::Scalar(255, 255, 255));

  // Apply the ROI_mask
  cv::Mat frame_masked;
  cv::bitwise_and(frame, frame, frame_masked, ROI_mask);

  

  cv::Mat img;
  if (debugSave){
    frame_masked.copyTo(img);
  }
    
  cv::Mat blurred;
  cv::GaussianBlur(frame_masked, blurred, cv::Size(11, 11), 0);
  cv::Mat mask;
  cv::cvtColor(blurred, mask, cv::COLOR_BGR2HSV);
  cv::inRange(mask, cv::Scalar(c_lb1, c_lb2, c_lb3), cv::Scalar(c_ub1, c_ub2, c_ub3), mask);

  // cv::inRange(mask, cv::Scalar(10, 100, 100), cv::Scalar(20, 255, 255), mask);
  // cv::erode(mask, mask, Mat, 2);
  // cv::dilate(mask, mask, Mat, 2);
  
  // toLog("start contour");
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::noArray(),cv::RETR_EXTERNAL,cv::CHAIN_APPROX_SIMPLE);
  // toLog("end contour");

  cv::Point2f center;
  float radius = 0;
  if (contours.size() > 0){
    float max_density = 0;
    cv::Point c(-1,-1);
    float r = 0.0;
    for (int i = 0; i < contours.size(); i++){
      // Fit enclosing circle and filter by min and max radius
      if (contours[i].size() > 0){
        cv::minEnclosingCircle(contours[i], center, radius);
        if ((radius > (arg_minRad > 0 ? arg_minRad : minRad)) && (radius < (arg_maxRad > 0 ? arg_maxRad : maxRad))){
          // std::cout << static_cast<int>(radius)<<std::endl;
          // std::cout << density << std::endl;

          float area = cv::contourArea(contours[i]);
          float density = area/(CV_PI*std::pow(radius,2));
          if (density > max_density && density > density_thr){
            max_density = density;
            c = center;
            r = radius;
          }

          
          cv::circle(img, center, static_cast<int>(radius), cv::Scalar(0,255,0), 2);
          cv::circle(img, center, 1, cv::Scalar(0, 0, 255), 2);
        }

      }
    }
    
      
    pos[0] = static_cast<int>(c.x);
    pos[1] = static_cast<int>(c.y);
      //

      // toLog("start save");
    if (debugSave){ 
      // paint found golfballs in image copy 'img'.
      // Draw circle and its center
      cv::circle(img, c, static_cast<int>(r), cv::Scalar(0,0,255), 2);
      cv::circle(img, c, 1, cv::Scalar(0, 0, 255), 2);
      // snprintf(s, MSL, "center: (%d, %d), radius: %d", center[0], centert[1], radius);
      // toLog(s);
      saveImageTimestamped(img, imgTime);
      saveImageTimestamped(mask, imgTime+1);
    }
    // toLog("end save");
    if (c.x == -1){
      toLog("No Circle with sufficent radius found");
      return false;
    }
    // toLog("end find golfball");
    return true;
  }
  return false;
}

bool Mgolfball::findGolfballHough(std::vector<int>& pos, cv::Mat *sourcePtr)
{ // taken from https://docs.opencv.org

  bool found = false;
  // Get frame 
  cv::Mat frame;
  if (sourcePtr == nullptr)
  {
    frame = cam.getFrameRaw();
    imgTime = cam.imgTime;
  }
  else
  {
    frame = *sourcePtr;
  }
  //
  if (frame.empty())
  {
    printf("MVision::findGolfball: Failed to get an image\n");
    return 0;
  }
  cv::Mat img;
  if (debugSave)
    frame.copyTo(img);
  //=============================================

  // filter
  // blur
  // convert colors
  // apply color filter
  // detect contours
  // count == no. of detected golfball candidates
  // set bool to true if no of contours greater than 0
  // choose closest
  
  cv::Mat blurred;
  cv::GaussianBlur(frame, blurred, cv::Size(11, 11), 0);
  cv::Mat mask;
  cv::cvtColor(blurred, mask, cv::COLOR_BGR2HSV);
  cv::inRange(mask, cv::Scalar(c_lb1, c_lb2, c_lb3), cv::Scalar(c_ub1, c_ub2, c_ub3), mask);
  //  cv::erode(mask, mask, Mat, 2);
  // cv::dilate(mask, mask, Mat, 2);
  
    
  vector<cv::Vec3f> circles;
  cv::HoughCircles( mask, circles, cv::HOUGH_GRADIENT, 1, hough_minDist, hough_p1, hough_p2, minRad, maxRad );

  if(circles.size() > 0){
    pos[0] = cvRound(circles[0][0]);
    pos[1] = cvRound(circles[0][1]);

    for( size_t i = 0; i < circles.size(); i++ )
    {
      if(debugSave){
        cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);
        // circle center
        cv::circle( img, center, 3, cv::Scalar(0,255,0), -1, 8, 0 );
        // circle outline
        cv::circle( img, center, radius, cv::Scalar(0,0,255), 3, 8, 0 );
      }
    }
    found = true;
  }else{
    found =  false;
  }
  if (debugSave){ 
  // snprintf(s, MSL, "center: (%d, %d), radius: %d", center[0], centert[1], radius);
  // toLog(s);
  saveImageTimestamped(img, imgTime);
  saveImageTimestamped(mask, imgTime+1);
  saveImageTimestamped(blurred, imgTime+2);
  }   

  return found;
  
}

void Mgolfball::saveImageInPath(cv::Mat& img, string name)
{ // Note, file type must be in filename
  const int MSL = 500;
  char s[MSL];
  // generate filename
  snprintf(s, MSL, "%s/%s", ini["golfball"]["imagepath"].c_str(), name.c_str());
  // save
  cv::imwrite(s, img);
  printf("# saved image to %s\n", s);
}


void Mgolfball::saveImageTimestamped(cv::Mat & img, UTime imgTime)
{
  const int MSL = 500;
  char s[MSL] = "golfball_";
  char * time_ptr = &s[strlen(s)];
  //
  imgTime.getForFilename(time_ptr);
  saveImageInPath(img, string(s) + ".jpg");
}


