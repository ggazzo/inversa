/**********************************************************************************************
 * Arduino PID Library - Version 1.2.1
 * by Brett Beauregard <br3ttb@gmail.com> brettbeauregard.com
 *
 * This Library is licensed under the MIT License
 **********************************************************************************************/

 #if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <PID_v1.h>

/*Constructor (...)*********************************************************
*    The parameters specified here are those for for which we can't set up
*    reliable defaults, so we need to have the user set them.
***************************************************************************/
PID::PID(double* Input, double* Output, double* Setpoint,
       double Kp, double Ki, double Kd, double POn, int ControllerDirection)
{
   myOutput = Output;
   myInput = Input;
   mySetpoint = Setpoint;
   inAuto = false;

   PID::SetOutputLimits(0, 255);				//default output limit corresponds to
                       //the arduino pwm limits

   SampleTime = 100;							//default Controller Sample Time is 0.1 seconds

   PID::SetControllerDirection(ControllerDirection);
   PID::SetTunings(Kp, Ki, Kd, POn);

   lastTime = millis()-SampleTime;
}

/*Constructor (...)*********************************************************
*    To allow backwards compatability for v1.1, or for people that just want
*    to use Proportional on Error without explicitly saying so
***************************************************************************/

PID::PID(double* Input, double* Output, double* Setpoint,
       double Kp, double Ki, double Kd, int ControllerDirection)
   :PID::PID(Input, Output, Setpoint, Kp, Ki, Kd, P_ON_E, ControllerDirection)
{

}


/* Compute() **********************************************************************
*     This, as they say, is where the magic happens.  this function should be called
*   every time "void loop()" executes.  the function will decide for itself whether a new
*   pid Output needs to be computed.  returns true when the output is computed,
*   false when nothing has been done.
**********************************************************************************/
bool PID::Compute()
{
  if(!inAuto) return false;
  unsigned long now = millis();
  unsigned long timeChange = (now - lastTime);
  if(timeChange>=SampleTime)
  {
     /*Compute all the working error variables*/
     double input = *myInput;
     double error = *mySetpoint - input;
     double dInput = (input - lastInput);
     outputSum+= (ki * error);

     /*Add Proportional on Measurement, if P_ON_M is specified*/
     if(pOnM)
       outputSum -= pOnMKp * dInput;


      outputSum = constrain(outputSum, outMin, outMax);

     /*Add Proportional on Error, if P_ON_E is specified*/
      double output = outputSum - kd * dInput;
      if(pOnE) output += pOnEKp * error;
      /*Compute Rest of PID Output*/

      output = constrain(output, outMin, outMax);

      *myOutput = output;

      /*Remember some variables for next time*/
      lastInput = input;
      lastTime = now;
      return true;
  }
  else return false;
}

/* SetTunings(...)*************************************************************
* This function allows the controller's dynamic performance to be adjusted.
* it's called automatically from the constructor, but tunings can also
* be adjusted on the fly during normal operation
******************************************************************************/
void PID::SetTunings(double Kp, double Ki, double Kd, double pOn)
{

  if (Kp<0 || Ki<0|| Kd<0 || pOn<0 || pOn>1) return;
  
   pOnE = pOn>0; //some p on error is desired;
   pOnM = pOn<1; //some p on measurement is desired;  
   

  // if (Kp<0 || Ki<0 || Kd<0) return;

  // pOn = POn;
  // pOnE = POn == P_ON_E;

  dispKp = Kp; dispKi = Ki; dispKd = Kd;

  double SampleTimeInSec = ((double)SampleTime)/1000;
  kp = Kp;
  ki = Ki * SampleTimeInSec;
  kd = Kd / SampleTimeInSec;

 if(controllerDirection ==REVERSE)
  {
     kp = (0 - kp);
     ki = (0 - ki);
     kd = (0 - kd);
  }


  pOnEKp = pOn * kp; 
  pOnMKp = (1 - pOn) * kp;
}

/* SetTunings(...)*************************************************************
* Set Tunings using the last-rembered POn setting
******************************************************************************/
void PID::SetTunings(double Kp, double Ki, double Kd){
   SetTunings(Kp, Ki, Kd, pOn); 
}

/* SetSampleTime(...) *********************************************************
* sets the period, in Milliseconds, at which the calculation is performed
******************************************************************************/
void PID::SetSampleTime(int NewSampleTime)
{
  if (NewSampleTime > 0)
  {
     double ratio  = (double)NewSampleTime
                     / (double)SampleTime;
     ki *= ratio;
     kd /= ratio;
     SampleTime = (unsigned long)NewSampleTime;
  }
}

/* SetOutputLimits(...)****************************************************
*     This function will be used far more often than SetInputLimits.  while
*  the input to the controller will generally be in the 0-1023 range (which is
*  the default already,)  the output will be a little different.  maybe they'll
*  be doing a time window and will need 0-8000 or something.  or maybe they'll
*  want to clamp it from 0-125.  who knows.  at any rate, that can all be done
*  here.
**************************************************************************/
void PID::SetOutputLimits(double Min, double Max)
{
  if(Min >= Max) return;
  outMin = Min;
  outMax = Max;

  if(inAuto)
  {
    if(*myOutput > outMax) *myOutput = outMax;
    else if(*myOutput < outMin) *myOutput = outMin;

    outputSum = constrain(outputSum, outMin, outMax);
  }
}

/* SetMode(...)****************************************************************
* Allows the controller Mode to be set to manual (0) or Automatic (non-zero)
* when the transition from manual to auto occurs, the controller is
* automatically initialized
******************************************************************************/
void PID::SetMode(int Mode)
{
   bool newAuto = (Mode == AUTOMATIC);
   if(newAuto && !inAuto)
   {  /*we just went from manual to auto*/
       PID::Initialize();
   }
   inAuto = newAuto;
}

/* Initialize()****************************************************************
*	does all the things that need to happen to ensure a bumpless transfer
*  from manual to automatic mode.
******************************************************************************/
void PID::Initialize()
{
  outputSum = *myOutput;
  lastInput = *myInput;
  if(outputSum > outMax) outputSum = outMax;
  else if(outputSum < outMin) outputSum = outMin;
}

/* SetControllerDirection(...)*************************************************
* The PID will either be connected to a DIRECT acting process (+Output leads
* to +Input) or a REVERSE acting process(+Output leads to -Input.)  we need to
* know which one, because otherwise we may increase the output when we should
* be decreasing.  This is called from the constructor.
******************************************************************************/
void PID::SetControllerDirection(int Direction)
{
  if(inAuto && Direction !=controllerDirection)
  {
     kp = (0 - kp);
     ki = (0 - ki);
     kd = (0 - kd);
  }
  controllerDirection = Direction;
}

/* Status Funcions*************************************************************
* Just because you set the Kp=-1 doesn't mean it actually happened.  these
* functions query the internal state of the PID.  they're here for display
* purposes.  this are the functions the PID Front-end uses for example
******************************************************************************/
double PID::GetKp(){ return  dispKp; }
double PID::GetKi(){ return  dispKi;}
double PID::GetKd(){ return  dispKd;}
int PID::GetMode(){ return  inAuto ? AUTOMATIC : MANUAL;}
int PID::GetDirection(){ return controllerDirection;}


void PID::SetSetpointWeight(double weight) {
    setpointWeight = weight;
}

// /**********************************************************************************************
//  * Arduino PID Library - Version 1.2.1
//  * by Brett Beauregard <br3ttb@gmail.com> brettbeauregard.com
//  *
//  * This Library is licensed under the MIT License
//  **********************************************************************************************/

// #if ARDUINO >= 100
//   #include "Arduino.h"
// #else
//   #include "WProgram.h"
// #endif

// #include <PID_v1.h>

// /*Constructor (...)*********************************************************
//  *    The parameters specified here are those for for which we can't set up
//  *    reliable defaults, so we need to have the user set them.
//  ***************************************************************************/
// PID::PID(double* Input, double* Output, double* Setpoint,
//         double Kp, double Ki, double Kd, int POn, int ControllerDirection)
// {
//     myOutput = Output;
//     myInput = Input;
//     mySetpoint = Setpoint;
//     inAuto = false;

//     PID::SetOutputLimits(0, 255);				//default output limit corresponds to
// 												//the arduino pwm limits

//     SampleTime = 100;							//default Controller Sample Time is 0.1 seconds

//     PID::SetControllerDirection(ControllerDirection);
//     PID::SetTunings(Kp, Ki, Kd, POn);

//     lastTime = millis()-SampleTime;

//     this->integral = 0;
//     this->lastError = 0;

//     this->pmax = 0;
//     this->alpha = 0.1;
//     this->integral_min = -255;
//     this->integral_max = 255;

// }

// /*Constructor (...)*********************************************************
//  *    To allow backwards compatability for v1.1, or for people that just want
//  *    to use Proportional on Error without explicitly saying so
//  ***************************************************************************/

// PID::PID(double* Input, double* Output, double* Setpoint,
//         double Kp, double Ki, double Kd, int ControllerDirection)
//     :PID::PID(Input, Output, Setpoint, Kp, Ki, Kd, P_ON_E, ControllerDirection)
// {

// }


// /* Compute() **********************************************************************
//  *     This, as they say, is where the magic happens.  this function should be called
//  *   every time "void loop()" executes.  the function will decide for itself whether a new
//  *   pid Output needs to be computed.  returns true when the output is computed,
//  *   false when nothing has been done.
//  **********************************************************************************/
// bool PID::Compute()
// {
//   // map from one code another

//   double input = *this->myInput;
//   double setpoint = *this->mySetpoint;
//   double lastError = this->lastError;
//   double Kp = this->kp;
//   double Ki = this->ki;
//   double Kd = this->kd;

//     if (!inAuto)
//       return false;
//     unsigned long now = millis();
//     unsigned long timeChange = (now - lastTime);
//     unsigned long SAMPLE_TIME_SECONDS = this->SampleTime / 1000;

//     float alpha = 0.1;

//     if (timeChange < this->SampleTime) { return false; } 
//     this->lastTime = now;
//     this->lastInput = input;

//     // Compute PID
//     double error = setpoint - input;

//     double errorDifference = (error - lastError);
//     double proportional = kp * error;
//     double derivative = errorDifference * this->kd / SAMPLE_TIME_SECONDS; // Derivative term
    
    
    
//     if(*this->myOutput > outMin && *this->myOutput < outMax) { // no need to accumulate integral if the output is operating in the limits
//       this->integral += ((error + this->lastError)/ 2) * this->ki * SAMPLE_TIME_SECONDS;
//       this->integral -= errorDifference * this->kp / SAMPLE_TIME_SECONDS;


//       // double Pmax = max(alpha * Pmax + (1 - alpha) * fabs(proportional), 1.0); 
//       // float scale = constrain(1.0 - fabs(proportional) / Pmax, 0, 1.0);
//       // this->integral *= scale;

//       this->integral = constrain(this->integral, this->integral_min, this->integral_max);
//     }

//     // Dynamically update Pmax using exponential moving average

//     // Scale integral reduction based on P

//     // if (error * this->lastError < 0) { 
//     //   integral *= 0.5; // Reduce the integral term when error crosses zero
//     // }
//     // auto steadyStateThreshold = 0.5;
//     // auto steadySamples = 5;
//     // auto steadyStartTime = 0;

//     // if (abs(error) <= steadyStateThreshold)
//     // {
//     //   if (steadyStartTime == 0) {
//     //       steadyStartTime = now; // Start timing when entering steady-state
//     //   }
//     //   if (now - steadyStartTime > steadySamples * this->SampleTime ) {
//     //       integral *= 0.90; // Slowly reduce integral over time
//     //   }
//     // }
//     // else
//     // {
//     //   steadyStartTime = 0; // Reset timer if error increases
//     // }

    
//     double output = proportional +  this->integral +  derivative;
//     this->lastError = error;
    
//     // if(output > outMax || output < outMin ) {
//     //   integral *= 0.9;
//     // }

//     *myOutput = constrain(output, outMin, outMax);;


//       // double input = *myInput;
//       // double error = *mySetpoint - input;
//       // double dInput = (input - lastInput);


//       // outputSum+= (ki * error);

//       // /*Add Proportional on Measurement, if P_ON_M is specified*/
//       // if(!pOnE) outputSum-= kp * dInput;


//       // // TODO: PUT THIS BACK
//       // // if(outputSum > outMax) outputSum= outMax;
//       // // else if(outputSum < outMin) outputSum= outMin;

//       // /*Add Proportional on Error, if P_ON_E is specified*/
//       // double output = 0;
//       // if(pOnE) output = kp * error;

//       // /*Compute Rest of PID Output*/
//       // output += outputSum - kd * dInput;
//       // output = constrain(output, outMin, outMax);
// 	    // *myOutput = output;

//       // /*Remember some variables for next time*/
//       // lastInput = input;
//       // lastTime = now;
// 	    return true;
// }

// /* SetTunings(...)*************************************************************
//  * This function allows the controller's dynamic performance to be adjusted.
//  * it's called automatically from the constructor, but tunings can also
//  * be adjusted on the fly during normal operation
//  ******************************************************************************/
// void PID::SetTunings(double Kp, double Ki, double Kd, int POn)
// {
//    if (Kp<0 || Ki<0 || Kd<0) return;

//    pOn = POn;
//    pOnE = POn == P_ON_E;

//    dispKp = Kp; dispKi = Ki; dispKd = Kd;

//    double SampleTimeInSec = ((double)SampleTime)/1000;
//    kp = Kp;
//    ki = Ki;
//    kd = Kd;
//   //  ki = Ki * SampleTimeInSec;
//   //  kd = Kd / SampleTimeInSec;

//   if(controllerDirection ==REVERSE)
//    {
//       kp = (0 - kp);
//       ki = (0 - ki);
//       kd = (0 - kd);
//    }

//    integral = 0;
// }

// /* SetTunings(...)*************************************************************
//  * Set Tunings using the last-rembered POn setting
//  ******************************************************************************/
// void PID::SetTunings(double Kp, double Ki, double Kd){
//     SetTunings(Kp, Ki, Kd, pOn); 
// }

// /* SetSampleTime(...) *********************************************************
//  * sets the period, in Milliseconds, at which the calculation is performed
//  ******************************************************************************/
// void PID::SetSampleTime(int NewSampleTime)
// {
//    if (NewSampleTime > 0)
//    {
//       // double ratio  = (double)NewSampleTime
//       //                 / (double)SampleTime;
//       // ki *= ratio;
//       // kd /= ratio;
//       SampleTime = (unsigned long)NewSampleTime;
//    }
// }

// /* SetOutputLimits(...)****************************************************
//  *     This function will be used far more often than SetInputLimits.  while
//  *  the input to the controller will generally be in the 0-1023 range (which is
//  *  the default already,)  the output will be a little different.  maybe they'll
//  *  be doing a time window and will need 0-8000 or something.  or maybe they'll
//  *  want to clamp it from 0-125.  who knows.  at any rate, that can all be done
//  *  here.
//  **************************************************************************/
// void PID::SetOutputLimits(double Min, double Max)
// {
//    if(Min >= Max) return;
//    outMin = Min;
//    outMax = Max;

//    if(inAuto)
//    {
// 	   if(*myOutput > outMax) *myOutput = outMax;
// 	   else if(*myOutput < outMin) *myOutput = outMin;

// 	   if(outputSum > outMax) outputSum= outMax;
// 	   else if(outputSum < outMin) outputSum= outMin;
//    }
// }

// /* SetMode(...)****************************************************************
//  * Allows the controller Mode to be set to manual (0) or Automatic (non-zero)
//  * when the transition from manual to auto occurs, the controller is
//  * automatically initialized
//  ******************************************************************************/
// void PID::SetMode(int Mode)
// {
//     bool newAuto = (Mode == AUTOMATIC);
//     if(newAuto && !inAuto)
//     {  /*we just went from manual to auto*/
//         PID::Initialize();
//     }
//     inAuto = newAuto;
// }

// /* Initialize()****************************************************************
//  *	does all the things that need to happen to ensure a bumpless transfer
//  *  from manual to automatic mode.
//  ******************************************************************************/
// void PID::Initialize()
// {
//    outputSum = *myOutput;
//    lastInput = *myInput;
//    this->outputSum = constrain(outputSum, outMin, outMax);

// }

// /* SetControllerDirection(...)*************************************************
//  * The PID will either be connected to a DIRECT acting process (+Output leads
//  * to +Input) or a REVERSE acting process(+Output leads to -Input.)  we need to
//  * know which one, because otherwise we may increase the output when we should
//  * be decreasing.  This is called from the constructor.
//  ******************************************************************************/
// void PID::SetControllerDirection(int Direction)
// {
//    if(inAuto && Direction !=controllerDirection)
//    {
// 	    kp = (0 - kp);
//       ki = (0 - ki);
//       kd = (0 - kd);
//    }
//    controllerDirection = Direction;
// }

// /* Status Funcions*************************************************************
//  * Just because you set the Kp=-1 doesn't mean it actually happened.  these
//  * functions query the internal state of the PID.  they're here for display
//  * purposes.  this are the functions the PID Front-end uses for example
//  ******************************************************************************/
// double PID::GetKp(){ return  dispKp; }
// double PID::GetKi(){ return  dispKi;}
// double PID::GetKd(){ return  dispKd;}
// double PID::GetIntegral(){ return  integral;}
// int PID::GetMode(){ return  inAuto ? AUTOMATIC : MANUAL;}
// int PID::GetDirection(){ return controllerDirection;}
