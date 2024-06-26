/*  
 * 
 * Copyright © 2023 DTU,
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

#include <string>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "mpose.h"
#include "steensy.h"
#include "uservice.h"
#include "sencoder.h"
#include "utime.h"
#include "cmotor.h"
#include "cservo.h"
#include "medge.h"
#include "cedge.h"
#include "cmixer.h"
#include "simu.h"

#include "bStairs.h"

// create class object
BStairs stairs;


void BStairs::setup()
{ // ensure there is default values in ini-file
  if (not ini["stairs"].has("log"))
  { // no data yet, so generate some default values
    ini["stairs"]["log"] = "true";
    ini["stairs"]["run"] = "true";
    ini["stairs"]["print"] = "true";
  }
  // get values from ini-file
  toConsole = ini["stairs"]["print"] == "true";
  //
  if (ini["stairs"]["log"] == "true")
  { // open logfile
    std::string fn = service.logPath + "log_stairs.txt";
    logfile = fopen(fn.c_str(), "w");
    fprintf(logfile, "%% Mission stairs logfile\n");
    fprintf(logfile, "%% 1 \tTime (sec)\n");
    fprintf(logfile, "%% 2 \tMission state\n");
    fprintf(logfile, "%% 3 \t%% Mission status (mostly for debug)\n");
  }

  servo.setServo(2, 1, -900, 200);
  
  setupDone = true;
}

BStairs::~BStairs()
{
  terminate();
}


void BStairs::run(bool entryDirectionStart, bool exitDirectionStart)
{
  if (not setupDone)
    setup();
  if (ini["stairs"]["run"] == "false")
    return;
  //
  UTime t("now");
  bool finished = false;
  bool lost = false;
  if(entryDirectionStart == true)
  {
    state = 0;
  }
    else
  {
    state = 0;
  }
  float f_LineWidth_Crossing = 0.08;

  float f_Line_LeftOffset = 0;
  float f_Line_RightOffset = 0;
  bool b_Line_HoldLeft = true;
  bool b_Line_HoldRight = false;
  float f_Velocity_DriveForward = 0.3; 
  float f_Velocity_DriveSlow = 0.2;
  float f_Velocity_DriveBack = -0.1;
  int servoDown = 400;
  int servoSpeed = 400;
  int steps = 0;
  oldstate = state;

  
  int woodWhite = 600;
  int blackWhite = 350;

  
  medge.updateCalibBlack(medge.calibBlack,8);
  medge.updatewhiteThreshold(blackWhite);

  //
  toLog("Plan Stairs started");
  //
  while (not finished and not lost and not service.stop)
  {
    switch (state)
    { // make a shift in heading-mission
      case 0:
        if(medge.edgeValid)
        {
          toLog("Started on a line");
          state = 3;
          pose.turned = 0.0;
          mixer.setVelocity(f_Velocity_DriveSlow);
          heading.setMaxTurnRate(3);
          mixer.setEdgeMode(b_Line_HoldLeft, 0.02);
        }
        else if(!medge.edgeValid)
        {
          toLog("Did not find Line");
          lost = true; 
        }
        break;
      // case 1:
      //     toLog("Start Driving - Start-side from Start intersection");
      //     mixer.setEdgeMode(b_Line_HoldRight, 0 );
      //     mixer.setVelocity(f_Velocity_DriveForward); 
      //     pose.dist = 0.0;
      //     pose.turned = 0.0;
      //     state = 2;
      //   break;
      // case 2:
      //     if(medge.edgeValid && (medge.width > f_LineWidth_Crossing))
      //     {
      //       toLog("Crossed intersection to seesaw - continue until next intersection");
      //       mixer.setVelocity(f_Velocity_DriveSlow);
      //       pose.dist = 0.0;
      //       pose.turned = 0.0;
      //       state = 3;
      //     }
      //   break;
      case 3:
      // toLog(std::to_string(medge.width).c_str());
          if(medge.edgeValid && (medge.width > f_LineWidth_Crossing)/* && pose.dist > 0.2*/)
          {
            toLog("Reached Stairs Intersection");
            pose.dist = 0;
            // pose.turned = 0;
            // heading.setMaxTurnRate(1);
            // mixer.setEdgeMode(b_Line_HoldLeft, 0.02);
            mixer.setVelocity(f_Velocity_DriveSlow);
            state = 4;
          }
        break;
      case 4:
        if(pose.dist > 0.6 /*|| pose.turned > M_PI/2*0.95*/){
          toLog("Reached start of Stairs, put down servo");
          mixer.setVelocity( 0.0 );
          // mixer.setEdgeMode(b_Line_HoldLeft, 0);
          pose.resetPose();
          servo.setServo(2, 1, servoDown, servoSpeed);
          t.clear();
          state = 5;
          // finished = true;
        }
        break;
      case 5:
        if(t.getTimePassed() > 4 || abs(servo.servo_position[1]- servoDown) <= 10){
          toLog("Servo Is Down, drive forward");
          mixer.setVelocity(f_Velocity_DriveSlow);
          pose.dist = 0;
          state = 6; 
        }
        break;
      case 6:
        // std::cout << abs(imu.acc[2]) << std::endl;
        if(pose.dist > 0.4 || (imu.acc[2] > 0 && pose.dist > 0.1))
        { 
          toLog("Down Step");          
          steps++;
          toLog(std::to_string(steps).c_str());    

          if(steps == 4){
            toLog("change calibration to wood");
            medge.updateCalibBlack(medge.calibWood,8);
            medge.updatewhiteThreshold(woodWhite);
          }

          state = 60; 
          
          // if(medge.edgeValid && medge.width > 0.02){
          //    if(steps < 5){
          //     // still on staircase
          //     state = 5;
          //     }
          //     else{
          //     // down and on line
          //     state = 8;
          //   }           
          // }else{
            
          // }
        }        
        break;

      case 60:
          pose.resetPose();
          mixer.setTurnrate(0);
          mixer.setVelocity(f_Velocity_DriveBack);
          servo.setServo(2, 1, 0, 1000);
          state = 7;
          t.clear();
        
        break;
      
      case 7:
        if(t.getTimePassed() > 3){
          toLog("Backed up against step");
          // mixer.setEdgeMode(b_Line_HoldLeft, 0);
          mixer.setVelocity(0.0);
          if(steps < 5){
            servo.setServo(2, 1, servoDown, servoSpeed);
            state = 5;
          }
          else{
            state = 8;
          }
        }
        break;

      case 8:
        mixer.setVelocity(0);
        pose.dist = 0;
        // finished = true;
        toLog("Down of staircase");
        servo.setServo(2, 1, -900, servoSpeed);
        pose.resetPose();

        // if(medge.edgeValid && medge.width > 0.02){
        //   toLog("Down of staircase - ONLINE");
        //   mixer.setEdgeMode(b_Line_HoldRight, 0.02); ////////// OFFSET
        // }else{
        //   toLog("Down of staircase - OFFLINE");
        //   pose.resetPose();
        // }

        mixer.setVelocity(f_Velocity_DriveSlow);
        state = 9;
        // if(medge.edgeValid)
        // {
        //   toLog("on valid edge, keep Line following");
        //   state = 9;
        // }else{
        //   toLog("not on valid edge, initiate turn");
        //   pose.dist = 0;
        //   pose.turned = 0;
        //   mixer.setVelocity(f_Velocity_DriveSlow);
        //   toLog("drive slowly forward");
        //   state = 10;
        // }
        break;

      case 9:
        if(pose.dist > 0.55){
          pose.resetPose();
          heading.setMaxTurnRate(1);
          mixer.setDesiredHeading(-M_PI/4);
          state = 10;
          pose.dist = 0;
        }
        break;

      case 10:
        if(medge.edgeValid && medge.width > 0.02 && pose.dist > 0.2){
          mixer.setVelocity(0.07);
          pose.resetPose();
          medge.updateCalibBlack(medge.calibWood,8);
          medge.updatewhiteThreshold(woodWhite);
          state = 11;
          
        }
        break;

      case 11:
        if(pose.dist > 0.05){
          pose.resetPose();
          mixer.setDesiredHeading(M_PI/4);
          state = 12;
        }
        break;

      case 12:
        if(abs(pose.turned) > M_PI/4*0.8 ){
          heading.setMaxTurnRate(3);
          mixer.setEdgeMode(b_Line_HoldLeft,0);
          mixer.setVelocity(f_Velocity_DriveSlow);
        }
        
        break;
      
      // case 10:      
      //   if(pose.dist > 0.1){
      //     toLog("stop and turn left");
      //     mixer.setVelocity(0);
      //     pose.resetPose();
      //     mixer.setDesiredHeading(1.57);
      //     state = 11;
      //   }
      //   break;
      // case 11:
      //   if(abs(pose.turned-(1.57)) < 0.1){
      //       toLog("drive slowly backward");
      //       pose.dist = 0;
      //       pose.turned = 0;
      //       mixer.setVelocity(f_Velocity_DriveBack);
      //       t.clear();
      //       state = 12;
      //   }
      //   break;

      // case 12:
      //   if(abs(pose.dist) < 0.5 || (t.getTimePassed() > 1)){
      //     toLog("against the wall, turning a bit right");
      //     mixer.setVelocity(f_Velocity_DriveSlow);
      //     pose.resetPose();
      //     mixer.setDesiredHeading(-0.2);            
      //     state = 13;
      //   }
      //   break;

      // case 13:
      // // can I call edge valid outside edge follow mode?
      //   if(medge.edgeValid){
      //     state = 9;
      //   }
      //   if(pose.dist > 0.5 && (!medge.edgeValid)){
      //     lost = true;
      //   }
      //   break;

      // case 9:
      //   toLog("on edge, drive slowly forward");        
      //   pose.dist = 0;
      //   pose.turned = 0;
      //   mixer.setEdgeMode(b_Line_HoldRight, 0);
      //   state = 14;
      //   break;

      // case 14:
      //   toLog("FINISHED: detected first intersection after staircase");      
      //   if(medge.edgeValid && (medge.width > f_LineWidth_Crossing)){
      //     finished = true;
      //   }
      //   break;


      default:
        toLog("Unknown state");
        lost = true;
        break;
    }
    if (state != oldstate)
    {
      oldstate = state;
      toLog("state start");
      // reset time in new state
      t.now();
    }
    // wait a bit to offload CPU
    usleep(4000);
  }
  if (lost)
  { // there may be better options, but for now - stop
    toLog("Plan Stairs got lost");
    mixer.setVelocity(0);
    mixer.setTurnrate(0);
  }
  else
    toLog("Plan Stairs finished");

  // servo.setServo(2,0);
}


void BStairs::terminate()
{ //
  if (logfile != nullptr)
    fclose(logfile);
  logfile = nullptr;
}

void BStairs::toLog(const char* message)
{
  UTime t("now");
  if (logfile != nullptr)
  {
    fprintf(logfile, "%lu.%04ld %d %% %s\n", t.getSec(), t.getMicrosec()/100,
            oldstate,
            message);
  }
  if (toConsole)
  {
    printf("%lu.%04ld %d %% %s\n", t.getSec(), t.getMicrosec()/100,
           oldstate,
           message);
  }
}

