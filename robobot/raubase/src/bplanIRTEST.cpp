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
#include "sdist.h"
#include "cheading.h"

#include "bplanIRTEST.h"

// create class object
BPlanIRTEST planIRTEST;

void BPlanIRTEST::setup()
{ // ensure there is default values in ini-file
  if (not ini["PlanIRTEST"].has("log"))
  { // no data yet, so generate some default values
    ini["PlanIRTEST"]["log"] = "true";
    ini["PlanIRTEST"]["run"] = "true";
    ini["PlanIRTEST"]["print"] = "true";
  }
  // get values from ini-file
  toConsole = ini["PlanIRTEST"]["print"] == "true";
  //
  if (ini["PlanIRTEST"]["log"] == "true")
  { // open logfile
    std::string fn = service.logPath + "log_PlanIRTEST.txt";
    logfile = fopen(fn.c_str(), "w");
    fprintf(logfile, "%% Mission PlanIRTEST logfile\n");
    fprintf(logfile, "%% 1 \tTime (sec)\n");
    fprintf(logfile, "%% 2 \tMission state\n");
    fprintf(logfile, "%% 3 \t%% Mission status (mostly for debug)\n");
  }
  setupDone = true;
}

BPlanIRTEST::~BPlanIRTEST()
{
  terminate();
}

void BPlanIRTEST::run(bool entryDirectionStart, bool exitDirectionStart)
{
  if (not setupDone)
    setup();
  if (ini["PlanIRTEST"]["run"] == "false")
    return;
  UTime t("now");
  bool finished = false;
  bool lost = false;
  
  if(entryDirectionStart == true)
  {
    state = 1;
  }
  else
  {
    state = 11; // should be 11.
  }

  oldstate = state;
  const int MSL = 100;
  char s[MSL];

  float speed = 0;
  
  //Hardcoded Line data
  //float f_LineWidth_MinThreshold = 0.02;
  //float f_LineWidth_NoLine = 0.01;
  float f_LineWidth_Crossing = 0.07;

  float f_Line_LeftOffset = 0.02;
  float f_Line_RightOffset = -0.02;
  bool b_Line_HoldLeft = true;
  bool b_Line_HoldRight = false;

  //Hardcoded time data
  //float f_Time_Timeout = 10.0;

  //Postion and velocity data
  float f_Velocity_DriveForward = 0.30; 
  //float f_Velocity_DriveBackwards = -0.15; 
  //float f_Distance_FirstCrossMissed = 1.5;
  float f_Distance_LeftCrossToRoundabout = 0.95;
  bool b_Flag_Once = true;
  //
  toLog("PlanIRTEST started");;
  toLog("Time stamp, IR dist 0, IR dist 1");
  //
  while (not finished and not lost and not service.stop)
  {
    switch (state)
    {
      /********************************************************************/
      /******************** Coming from the start-side ********************/
      /********************************************************************/
      //Case 1 - first crossing on the track and forward

      case 0: 
        
        if(b_Flag_Once){
          mixer.setVelocity(0);
          pose.resetPose();
          mixer.setTurnrate(0.5);
          b_Flag_Once = false;
        }
      break;


      case 1:
          // start SS
          mixer.setEdgeMode(b_Line_HoldLeft, f_Line_LeftOffset );
          mixer.setVelocity(f_Velocity_DriveForward); //Already driving
          toLog("Starting from split - Start-side of roundabout");
          pose.dist=0;   
          state = 2;
        break;
      
      //Case 2 - Drive 0.9 still following line. Then start turning.
      case 2:  // Stop goal reached case
        if(pose.dist > f_Distance_LeftCrossToRoundabout)
        { 
          pose.dist=0;   
          mixer.setEdgeMode(b_Line_HoldLeft, f_Line_LeftOffset );
          toLog("Ready to enter the roundabout from the start-side");
          mixer.setVelocity(0);
          pose.resetPose();
          heading.setMaxTurnRate(1.0);
          mixer.setDesiredHeading(0.8); // (3*pi) / 8
          state = 3;
        }
        break;
      
      //Case 3 - Stop turning after some angle
      case 3:
        //toLog(std::to_string(pose.turned).c_str());
        if(abs(pose.turned) > 0.8-0.02){
          mixer.setTurnrate(0);
          state = 21;
        }
      break;

      /******************************************************************/
      /******************** Coming from the axe-side ********************/
      /********************************************************************/
      //Case 11 - Drive from the Axe-side
      case 11: 
          toLog("Axe side started");
          pose.resetPose();
          mixer.setDesiredHeading(-0.8);
          state = 12;
        
      break;
      
      case 12:
        if(abs(pose.turned) > 0.78)
        {
          state = 13;
        } 
      break;

      //Case 21 - If robot is seen, clear time and wait until case 6
      case 13: 
        if(dist.dist[1] < 0.35){
          toLog("Robot seen!");
          t.clear();
          pose.dist = 0;
          state = 14;
        }
      break;  

      //Case 22 - After 2 seconds, reset distance, set follow line mode and drive forward slowly.
      case 14: 
        //toLog(std::to_string(t.getTimePassed()).c_str());
        if(t.getTimePassed() > 5)
        {
          pose.dist = 0;
          mixer.setVelocity(0.1);
          state = 15;
        }
      break;

      case 15:
        if(abs(pose.dist) > 0.15)
        {
          mixer.setVelocity(0);
          pose.resetPose();
          mixer.setDesiredHeading(0.8);
          state = 16;
        } 
      break;

      case 16:
        if(abs(pose.turned) > 0.78)
        {
          mixer.setVelocity(0.2);
          state = 95;
        } 
      break;

      /******************************************************************/
      /**** Both sides collected behaviour from waiting on the robot ****/
      /********************************************************************/
      //Case 21 - If robot is seen, clear time and wait until case 6
      case 21: 
        // toLog(std::to_string(dist.dist[0]).c_str()); print out side sensor
        //toLog(std::to_string(dist.dist[1]).c_str());
        if(dist.dist[1] < 0.35){ // dist.dist should be 1 !!
          toLog("Robot seen!");
          t.clear();
          pose.dist = 0;
          pose.resetPose();
          mixer.setDesiredHeading(-0.75);
          state = 22;
        }
      break;

      //Case 22 - After 2 seconds, reset distance, set follow line mode and drive forward slowly.
      case 22: 
        //toLog(std::to_string(t.getTimePassed()).c_str());
        if(abs(pose.turned) > 0.75 - 0.02 && medge.edgeValid)
        {
          pose.dist = 0;
          pose.turned = 0;
          heading.setMaxTurnRate(3);
          mixer.setEdgeMode(b_Line_HoldRight,f_Line_RightOffset); 
          mixer.setVelocity(0.3);
          state = 94;
        }
      break;

      case 94:
        if(medge.width > 0.09)
        {
          mixer.setVelocity(0.0);
          pose.resetPose();
          heading.setMaxTurnRate(1);
          mixer.setDesiredHeading(1.57);
          state = 95;
        }
      break;
      case 95:
        if(abs(pose.turned) > (1.57 - 0.02))
        {
          mixer.setVelocity(0.3);
          state = 96;
        }
      break;
      case 96:
          if(pose.dist > 0.1  && medge.width > 0.06){
            pose.resetPose();
            mixer.setVelocity(0.3);
            state = 97;
          }

      break;

      case 97:
        if(pose.dist > 0.05){
        pose.resetPose();
        mixer.setVelocity(0.0);
        mixer.setDesiredHeading(1.57);
        state = 98;
        }
        break;
      
      case 98:
        if (abs(pose.turned) > 1.57 - 0.02)
        {
          pose.dist = 0;
          pose.turned = 0;
          heading.setMaxTurnRate(3);
          mixer.setEdgeMode(b_Line_HoldRight,f_Line_RightOffset);
          mixer.setVelocity(0.15);
          state = 99;
        }        
      break;

      case 99:
        if(pose.dist > 0.25)
        {
          heading.setMaxTurnRate(3);
          mixer.setVelocity(0.30);
          mixer.setEdgeMode(b_Line_HoldLeft, f_Line_LeftOffset );
          pose.dist = 0;
          state = 23;
        }
      break;

      //Case 23 - After 0.6 driven, set up speed, follow right with new offset and go to case 8
      case 23:
        if(pose.dist > 0.55)
        {
          mixer.setVelocity(0.35);
          mixer.setEdgeMode(b_Line_HoldRight, -0.01 );
          state = 24;
        }

      break;

      //Case 24 - At crossing, attempt to go into roundabout
      case 24:
      if(medge.width > f_LineWidth_Crossing) //0.07
        { 
          mixer.setVelocity(0.30);
          //pose.resetPose();
          toLog("Make 3 test");
          pose.dist = 0;
          //state = 25;
          state = 25;
        }
        break;

      case 25:
      if(pose.dist > 0.7) //0.07
        { 
          
          mixer.setVelocity(0);
          //pose.resetPose();
          toLog("Make 3 test");
          pose.resetPose();
          heading.setMaxTurnRate(1);
          mixer.setDesiredHeading(1.28);
          //state = 25;
          state = 26;
        }
      break;
      
      //Case 25 - After turning some angle, go to state 10
      case 26:
      //TEST IF WE CAN DRIVE NEGATIVE AND GO BACKWARDS
      //toLog("Turned");
      //toLog(std::to_string(pose.turned).c_str());
      //toLog("Turnerate");
      //toLog(std::to_string(pose.turnrate).c_str());


        if(abs(pose.turned) > 1.28-0.02){
          speed  = 0;
          state = 27;
        }
      break;

      case 27:
        if(speed > -0.45) //RAMP DOWN BEFORE TURN
        {
          speed = speed - 0.003;
          mixer.setVelocity(speed);
        }
        else
        {
          state = 31;
        }
      break;

      /********************************************************************/
      /************************ On the roundabout *************************/
      /********************************************************************/
      //Case 31 - after some distance, stop and turn for the circle
      case 31:
        if((abs(pose.dist) > 0.55))
        {
          pose.resetPose();
          mixer.setVelocity(0); //TEST THIS!!!!!
          mixer.setDesiredHeading(1.6);
          state = 32;
          t.clear();
        } 
      break;

      //Case 32 - After turning some angle, ready for turning around on the roundabout. 
      case 32:
      //TEST IF WE CAN DRIVE NEGATIVE AND GO BACKWARDS
        if(abs(pose.turned) > 1.58 )
        {
          mixer.setDesiredHeading(0);
          pose.turned = 0;
          t.clear();
          mixer.setTurnrate(1.1);
          mixer.setVelocity(0.35);
          //These work fucking good
          //mixer.setTurnrate(0.55);
          //mixer.setVelocity(0.4);
          state = 33;
        } 
      break;

      //Case 27 - After turning some angle, ready for turning around on the roundabout. 
      case 33:
      //TEST IF WE CAN DRIVE NEGATIVE AND GO BACKWARDS
        if(abs(pose.turned) > 6.3)
        {
          mixer.setVelocity(0);
          mixer.setTurnrate(0);
          pose.resetPose();
          mixer.setDesiredHeading(-1.55);
          /*pose.resetPose();
          mixer.setTurnrate(0.75);
          mixer.setVelocity(0.4);
          t.clear();*/
          state = 34;
        } 
      break;

      case 34:
      //TEST IF WE CAN DRIVE NEGATIVE AND GO BACKWARDS
        if(abs(pose.turned) > 1.55-0.02)
        {
          mixer.setVelocity(0.1);
          state = 35;
        } 
      break;

      case 35:
      //TEST IF WE CAN DRIVE NEGATIVE AND GO BACKWARDS
        if(abs(pose.dist) > 0.08)
        {
          mixer.setVelocity(0);
            state = 41;
        } 
      break;

        //else if(t.getTimePassed() > 5)
        //{
          //some fail saving code.
          //mixer.setVelocity(-0.2);
        //}
      break;

      /********************************************************************/
      /*************************** Exit strategy **************************/
      /********************************************************************/
      //Case 41 - If robot is seen, clear time and wait until case 6
      case 41: 
      toLog(std::to_string(dist.dist[1]).c_str());
        if(dist.dist[1] < 0.35)
        {
          toLog("Robot seen!");
          heading.setMaxTurnRate(3);
          t.clear();
          state = 42;
        }
      break;

      //Case 42 - If robot is seen, clear time and wait until case 6
      case 42:
        if(t.getTimePassed() > 4)
        {
          mixer.setVelocity(0.1);
          state = 43;
        }
      break;


      case 43:
      if(medge.width > f_LineWidth_Crossing) 
        { 
          // start driving
          toLog("First line found");
          pose.dist = 0;
          state = 44;  
        }
      break;

      //Turn left or right depending on exitDirectionStart boolean
      case 44:
      if((medge.width > f_LineWidth_Crossing) && (pose.dist >= 0.05)) 
        { 
          // start driving
          toLog("Second line found");
          pose.resetPose();
          heading.setMaxTurnRate(1);
          if(exitDirectionStart)
          {
            mixer.setDesiredHeading(-1.6);
          }
          else
          {
            mixer.setDesiredHeading(1.6);
          }
          state = 45;  
        }
      break;

      //When turned enough, drive forward and follow line
      case 45:
        if(abs(pose.turned) > 1.58)
        {
          mixer.setVelocity(0.2);
          heading.setMaxTurnRate(3);
          mixer.setEdgeMode(b_Line_HoldRight, -0.02 );
          state = 46;
        }
      break;

      case 46:
        if(exitDirectionStart) // false = axe, true = cross
        {
          state = 51;
        }
        else
        {
           state = 61;
        }
      break;
      
      case 51:
       if(medge.width > f_LineWidth_Crossing) 
        { 
          mixer.setVelocity(0);
          pose.resetPose();
          mixer.setDesiredHeading(3);
          
          state = 52;
        }
      break;

      case 52:
        if(abs(pose.turned) > 2.98)
        {
          pose.dist = 0;
          mixer.setVelocity(-0.1);
          state = 53;
        }
      break;

      case 53:
        if(abs(pose.dist) > 0.3)
        {
          mixer.setVelocity(0);
          toLog("FINISH at Start :)");
          finished = true;
        }
      break;
      
      case 61: 
       if(medge.width > f_LineWidth_Crossing) 
        { 
          pose.resetPose();
          mixer.setVelocity(0.3);
          state = 62;
        }
      break;

      case 62:
        if(pose.dist > 0.1)
        {
          mixer.setEdgeMode(b_Line_HoldRight, -0.02 );
          state = 63;
        } 
      break;

      case 63:
        if(medge.width > 0.07){
          mixer.setVelocity(0);
          finished = true;
          toLog("Finished at Axe");
        }
      break;
        break;



      case 999: // IR dist case 
        float irDist0;
        float irDist1;

        while(t.getTimePassed() < 10){
            irDist0 = dist.dist[0];
            irDist1 = dist.dist[1];
            std::string temp_str = std::to_string(t.getTimePassed()) 
                + " " + std::to_string(irDist0)
                + " " + std::to_string(irDist1);
            toLog(temp_str.c_str());
        };
        finished = true;    
        break;

      default:
        toLog("Default Mission 0");
        lost = true;
      break;
    }
    if (state != oldstate)
    { // C-type string print
      snprintf(s, MSL, "State change from %d to %d", oldstate, state);
      toLog(s);
      oldstate = state;
      t.now();
    }
    // wait a bit to offload CPU (4000 = 4ms)
    usleep(4000);
  }
  if (lost)
  { // there may be better options, but for now - stop
    toLog("PlanIRTEST got lost - stopping");
    mixer.setVelocity(0);
    mixer.setTurnrate(0);
  }
  else
    toLog("PlanIRTEST finished");
}


void BPlanIRTEST::terminate()
{ //
  if (logfile != nullptr)
    fclose(logfile);
  logfile = nullptr;
}

void BPlanIRTEST::toLog(const char* message)
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
