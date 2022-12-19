/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
** ****************************************************************** */
//
//
#include <g3_api.h>
#undef G3_Runtime
#include "G3_Runtime.h"
#include "Logging/G3_Logging.h"
#include <handler/OPS_Stream.h>
#include <StandardStream.h>      

#include <stdio.h>
#ifdef _WIN32
#  include <io.h>
#  define isatty _isatty
#  define STDERR_FILENO _fileno(stdout)
#else
#  include <unistd.h>               
#endif

extern int OpenSeesAppInit(Tcl_Interp *interp);
extern void G3_InitTclSequentialAPI(Tcl_Interp* interp);
extern int init_g3_tcl_utils(Tcl_Interp*);

extern "C" {

//
// Called when the library is loaded as a Tcl extension.
//
int DLLEXPORT
Openseesrt_Init(Tcl_Interp *interp)
{
  if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "OpenSeesRT", "0.0.1") == TCL_ERROR) {
    return TCL_ERROR;
  }

  G3_Runtime *rt = new G3_Runtime{interp};
  Tcl_SetAssocData(interp, "G3_Runtime", NULL, (ClientData)rt);

  OpenSeesAppInit(interp);
  G3_InitTclSequentialAPI(interp);
  init_g3_tcl_utils(interp);

  if (isatty(STDERR_FILENO))
    G3_setStreamColor(nullptr, G3_Warn, 1);
  return TCL_OK;
}

} // extern "C"

