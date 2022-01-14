

#include <g3_api.h>


#include <SRC/material/uniaxial/stiffness/ConstantStiffnessDegradation.h>
void *OPS_ConstantStiffnessDegradation(void)
{
  StiffnessDegradation *theDegradation = 0;

  if (OPS_GetNumRemainingInputArgs() < 3) {
    opserr << "Invalid number of args, want: stiffnessDegradation Constant "
              "tag? alpha? beta?"
           << endln;
    return 0;
  }

  int iData[1];
  double dData[2];

  int numData = 1;
  if (OPS_GetIntInput(&numData, iData) != 0) {
    opserr << "WARNING invalid tag for stiffnessDegradation Constant" << endln;
    return 0;
  }

  numData = 2;
  if (OPS_GetDoubleInput(&numData, dData) != 0) {
    opserr << "WARNING invalid data for stiffnessDegradation Constant" << endln;
    return 0;
  }

  theDegradation =
      new ConstantStiffnessDegradation(iData[0], dData[0], dData[1]);
  if (theDegradation == 0) {
    opserr << "WARNING could not create ConstantStiffnessDegradation\n";
    return 0;
  }

  return theDegradation;
}
