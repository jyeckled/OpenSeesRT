

#include <g3_api.h>


#include <SRC/element/fourNodeQuad/SixNodeTri.h>
void *OPS_SixNodeTri()
{
  int ndm = OPS_GetNDM();
  int ndf = OPS_GetNDF();

  if (ndm != 2 || ndf != 2) {
    opserr << "WARNING -- model dimensions and/or nodal DOF not compatible "
              "with quad element\n";
    return 0;
  }

  if (OPS_GetNumRemainingInputArgs() < 10) {
    opserr << "WARNING insufficient arguments\n";
    opserr << "Want: element SixNodeTri eleTag? iNode? jNode? kNode? lNode? "
              "nNode5 mNode6 thk? type? matTag? <pressure? rho? b1? b2?>\n";
    return 0;
  }

  // SixNodeTriId, iNode, jNode, kNode, lNode
  // nNode, mNode
  int idata[7];
  int num = 7;
  if (OPS_GetIntInput(&num, idata) < 0) {
    opserr << "WARNING: invalid integer inputs\n";
    return 0;
  }

  double thk = 1.0;
  num = 1;
  if (OPS_GetDoubleInput(&num, &thk) < 0) {
    opserr << "WARNING: invalid double inputs\n";
    return 0;
  }

  const char *type = OPS_GetString();

  int matTag;
  num = 1;
  if (OPS_GetIntInput(&num, &matTag) < 0) {
    opserr << "WARNING: invalid matTag\n";
    return 0;
  }

  NDMaterial *mat = OPS_getNDMaterial(matTag);
  if (mat == 0) {
    opserr << "WARNING material not found\n";
    opserr << "Material: " << matTag;
    opserr << "\nSixNodeTri element: " << idata[0] << endln;
    return 0;
  }

  // p, rho, b1, b2
  double data[4] = {0, 0, 0, 0};
  num = OPS_GetNumRemainingInputArgs();
  if (num > 4) {
    num = 4;
  }
  if (num > 0) {
    if (OPS_GetDoubleInput(&num, data) < 0) {
      opserr << "WARNING: invalid integer data\n";
      return 0;
    }
  }

  return new SixNodeTri(idata[0], idata[1], idata[2], idata[3], idata[4],
                        idata[5], idata[6], *mat, type, thk, data[0], data[1],
                        data[2], data[3]);
}
