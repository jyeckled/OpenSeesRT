//===----------------------------------------------------------------------===//
//
//        OpenSees - Open System for Earthquake Engineering Simulation    
//
//===----------------------------------------------------------------------===//
//
// Description: This file contains the function to parse the Tcl input
//              for prismatic frame elements
//
// Written: fmk,cmp
// Created: 07/99
//
#include <tcl.h>
#include <array>
#include <vector>
#include <cstddef>
#include <stdlib.h>
#include <string.h>
#include <Domain.h>
#include <Logging.h>
#include <ArgumentTracker.h>
#include <FrameTransform.h>
#include <LinearFrameTransf3d.h>
#include "ElasticBeam2d.h"
#include "ElasticBeam3d.h"

#include "PrismFrame2d.h"
#include "PrismFrame3d.h"

#include <FrameSection.h>
#include <SectionForceDeformation.h>

#include <BasicModelBuilder.h>

// enum class Position : int {
//   A, E, Iz, Iy, G, J, Transform, End
// };

enum class Args2D : int {
  A, E, Iz, Transform, End, 
  // values coming after End wont be handled
  // by ArgumentTracker; these need to be here for
  // the template to compile.
  G, J, Iy
};

enum class Args3D : int {
  A, E, G, J, Iy, Iz, Transform, End
};

//
// element NAME 
//    $tag $iNode $jNode
//    $E $G $A $J $Iy $Iz $Avy $Avz
//    [$tag | -transform $tag | -vert $x $y $z]
//    < -delta $delta >
//    < -shear $Avy $Avz [$G] >
//    < -mass $m> < -cMass >
//    < -warp $warp>
//
// For a two-dimensional problem:
// element elasticBeamColumn $eleTag $iNode $jNode $A $E $Iz $transfTag <-mass $massDens> <-cMass>
//
// For a three-dimensional problem:
// 
// element elasticBeamColumn $eleTag $iNode $jNode $A $E $G $J $Iy $Iz $transfTag <-mass $massDens> <-cMass>
//
// For two- and three-dimensional problems:
// element elasticBeamColumn $eleTag $iNode $jNode $secTag $transfTag <-mass $massDens> <-cMass>
//
template <typename Position>
int
Parse_ElasticBeam(ClientData clientData, Tcl_Interp *interp, int argc,
                               TCL_Char ** const argv)
{

  // The program is fundamentally incorrect if this function
  // is invoked with NULL clientData
  assert(clientData != nullptr);
  BasicModelBuilder *builder = (BasicModelBuilder*)clientData;

  //
  // Declare the variables we're parsing
  //
  struct {
    double E, G;                         // Material data
    double A, Iz, Iy, Ixy, J, Cw;        // Section data
    double thermal_coeff = 0.0,          // Thermal
           thermal_depth = 0.0;
  } beam_data;
  double mass = 0.0;
  bool use_mass = false;

  ArgumentTracker<Position> tracker;

  struct BeamFlags {
    int                                  shear_flag = 0;
    int                                  geom_flag  = 0;
    int                                  relz_flag  = 0;
    int                                  rely_flag  = 0;
    enum  {NoWarp=0, HaveWarp}           warp_flag  = NoWarp;
    enum  {LumpedMass=0, CubicMass} mass_flag  = LumpedMass;
  } options;

  std::vector<int> positional;


  int tag, iNode, jNode, transTag;

  FrameTransform2d *theTrans2d = nullptr;
  FrameTransform3d *theTrans3d = nullptr;
  FrameSection* theSection     = nullptr;


  Element *theBeam = nullptr;

  int ndm = builder->getNDM();
  int ndf = builder->getNDF();

  //
  // Parse common parameters
  //
  if (argc < 5) {
    opserr << OpenSees::PromptValueError << "insufficient number of arguments.\n";
    return TCL_ERROR;
  }

  // Parse tag, iNode, jNode
  if (Tcl_GetInt(interp, argv[2], &tag) != TCL_OK) {
    opserr << OpenSees::PromptValueError << "invalid tag " << argv[2] << "\n";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[3], &iNode) != TCL_OK) {
    opserr << OpenSees::PromptValueError << "invalid iNode" << tag << "\n";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[4], &jNode) != TCL_OK) {
    opserr << OpenSees::PromptValueError << "invalid jNode" << tag << "\n";
    return TCL_ERROR;
  }

  //
  // Parse keyword arguments
  //
  for (int argi = 5; argi < argc; argi++) {

    // Section
    if (strcmp(argv[argi], "-section") == 0) {
      if (argc < argi + 2) {
        opserr << OpenSees::PromptValueError
               << "not enough arguments, expected -section $section?\n";
        return TCL_ERROR;
      }

      int section;
      if (Tcl_GetInt(interp, argv[argi+1], &section) != TCL_OK) {
        opserr << OpenSees::PromptValueError 
               << "invalid section tag.\n";
        return TCL_ERROR;
      }

      theSection = builder->getTypedObject<FrameSection>(section);
      if (theSection == nullptr)
        return TCL_ERROR;

      argi += 1;
      while (tracker.current() != Position::Transform)
        tracker.increment();
    }

    // Transform
    else if (strcmp(argv[argi], "-transform") == 0) {
      if (argc < argi + 2) {
        opserr << OpenSees::PromptValueError
               << "not enough arguments, expected -transform $transform?\n";
        return TCL_ERROR;
      }

      int transform;
      if (Tcl_GetInt(interp, argv[argi+1], &transform) != TCL_OK) {
        opserr << OpenSees::PromptValueError 
               << "invalid transform tag.\n";
        return TCL_ERROR;
      }
      if (ndm == 2) {
        theTrans2d = builder->getTypedObject<FrameTransform2d>(transform);
        if (theTrans2d == nullptr)
          return TCL_ERROR;
      }
      else if (ndm == 3) {
        theTrans3d = builder->getTypedObject<FrameTransform3d>(transform);
        if (theTrans3d == nullptr)
          return TCL_ERROR;
      }
      argi += 1;
      tracker.consume(Position::Transform);
    }
    else if (strcmp(argv[argi], "-vertical") == 0) {
      if (argc < argi + 2) {
        opserr << OpenSees::PromptValueError
               << "not enough arguments, expected -vertical $vertical?\n";
        return TCL_ERROR;
      }
      int yargc;
      TCL_Char ** yargv;
      if (Tcl_SplitList(interp, argv[argi+1], &yargc, &yargv) != TCL_OK) {
        opserr << G3_ERROR_PROMPT << "could not split list\n";
        return TCL_ERROR;
      }
      double vertical[3];
      for (int j=0; j<3; j++)
        if (Tcl_GetDouble(interp, yargv[j], &vertical[j]) != TCL_OK) {
          opserr << G3_ERROR_PROMPT << "could not read vector\n";
          return TCL_ERROR;
        }
      theTrans3d = new LinearFrameTransf3d(0, Vector(vertical,3));
      builder->addTaggedObject<FrameTransform3d>(*theTrans3d);
      Tcl_Free((char *)yargv);
      argi += 1;
      tracker.consume(Position::Transform);
    }

    // Mass density
    else if (strcmp(argv[argi], "-mass") == 0) {
      if (argc < argi + 2) {
        opserr << OpenSees::PromptValueError << "not enough arguments, expected -mass $mass?\n";
        return TCL_ERROR;
      }
      if (Tcl_GetDouble(interp, argv[argi + 1], &mass) != TCL_OK) {
        opserr << OpenSees::PromptValueError
               << "invalid mass\n";
        return TCL_ERROR;
      }
      // If mass is specified directly in the element command, tell the element
      // not to ask the cross section for it's mass.
      use_mass = true;
      argi += 1;
    }

    // Geometry flag
    else if (strcmp(argv[argi], "-order") == 0) {
      if (argc < argi + 2) {
        opserr << OpenSees::PromptValueError
               << "not enough arguments, expected -delta $delta\n";
        return TCL_ERROR;
      }
      if (Tcl_GetInt(interp, argv[argi + 1], &options.geom_flag) != TCL_OK) {
        opserr << OpenSees::PromptValueError
               << "invalid delta\n";
        return TCL_ERROR;
      }
      argi += 1;

    // Mass flags
    }
    else if ((strcmp(argv[argi], "-lMass") == 0) ||
               (strcmp(argv[argi], "lMass") == 0)) {
      options.mass_flag = BeamFlags::LumpedMass;
    }
    else if ((strcmp(argv[argi], "-cMass") == 0) ||
               (strcmp(argv[argi], "cMass") == 0)) {
      options.mass_flag = BeamFlags::CubicMass;
    }

    // Thermal
    else if (strcmp(argv[argi], "-alpha") == 0) {
      if (argc == ++argi ||
          Tcl_GetDouble(interp, argv[argi], &beam_data.thermal_coeff) != TCL_OK) {
        opserr << OpenSees::PromptValueError
               << "invalid thermal alpha" << tag
               << " iNode jNode A E I alpha depth \n";
        return TCL_ERROR;
      }
    }

    else if (strcmp(argv[argi], "-depth") == 0) {
      if (argc == ++argi ||
          Tcl_GetDouble(interp, argv[argi], &beam_data.thermal_depth) != TCL_OK) {
        opserr << OpenSees::PromptValueError 
               << "invalid thermal depth" << tag
               << " iNode jNode A E I alpha depth \n";
        return TCL_ERROR;
      }
    } 

    else
      positional.push_back(argi);
  }

  //
  // Parse positional arguments
  //
  if (positional.size() <= 2) {
    std::size_t i=0;
    // Section
    if (theSection == nullptr) {
      if (i >= positional.size()) {
        opserr << OpenSees::PromptValueError
               << "Missing required positional argument $section\n";
        return TCL_ERROR;
      }
      int section;
      if (Tcl_GetInt(interp, argv[positional[i]], &section) != TCL_OK) {
        opserr << OpenSees::PromptValueError << "invalid secTag\n";
        return TCL_ERROR;
      }
      theSection = builder->getTypedObject<FrameSection>(section);
      if (theSection == nullptr)
        return TCL_ERROR;

      // Fast-forward through section parameters
      while (tracker.current() != Position::Transform)
        tracker.increment();
      i++;
    }

    // Coordinate transformation
    if (tracker.contains(Position::Transform)) {
      if (i >= positional.size()) {
        opserr << OpenSees::PromptValueError
               << "Missing required positional argument $transform\n";
        return TCL_ERROR;
      }
      if (Tcl_GetInt(interp, argv[positional[i]], &transTag) != TCL_OK) {
        opserr << OpenSees::PromptValueError << "invalid transTag" << tag;
        opserr << " iNode jNode sectionTag? transfTag?\n";
        return TCL_ERROR;
      }
      // Check that the builder has a transform with tag; error will be
      // printed from builder
      if (ndm == 2) {
        theTrans2d = builder->getTypedObject<FrameTransform2d>(transTag);
        if (theTrans2d == nullptr)
          return TCL_ERROR;
      }
      else if (ndm == 3) {
        theTrans3d = builder->getTypedObject<FrameTransform3d>(transTag);
        if (theTrans3d == nullptr)
          return TCL_ERROR;
      }
      tracker.consume(Position::Transform);
    }
  }

  else {
    // Parse remaining section properties as positional arguments

    for (int i : positional) {

      switch (tracker.current()) {
        case Position::E:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.E) != TCL_OK) {
              opserr << OpenSees::PromptParseError << "invalid E\n";
              return TCL_ERROR;
          } else {
            tracker.increment();
            break;
          }

        case Position::G:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.G) != TCL_OK) {
              opserr << OpenSees::PromptParseError << "invalid G\n";
              return TCL_ERROR;
          } else {
            tracker.increment();
            break;
          }

        case Position::A:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.A) != TCL_OK) {
              opserr << OpenSees::PromptParseError << "invalid A\n";
              return TCL_ERROR;
          } else {
            tracker.increment();
            break;
          }

        case Position::Iz:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.Iz) != TCL_OK) {
              opserr << OpenSees::PromptParseError << "invalid Iz\n";
              return TCL_ERROR;
          }
          tracker.increment();
          break;

        case Position::Iy:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.Iy) != TCL_OK || beam_data.Iy < 0) {
              opserr << OpenSees::PromptParseError << "invalid Iy\n";
              return TCL_ERROR;
          }
          tracker.increment();
          break;


        case Position::J:
          if (Tcl_GetDouble (interp, argv[i], &beam_data.J) != TCL_OK) {
              opserr << OpenSees::PromptParseError << "invalid J\n";
              return TCL_ERROR;
          }
          tracker.increment();
          break;

        case Position::Transform:
          if (Tcl_GetInt(interp, argv[i], &transTag) != TCL_OK) {
            opserr << OpenSees::PromptValueError << "invalid transTag\n";
            return TCL_ERROR;
          }

          if (ndm == 2) {
            theTrans2d = builder->getTypedObject<FrameTransform2d>(transTag);
            if (theTrans2d == nullptr)
              return TCL_ERROR;
          }
          else if (ndm == 3) {
            theTrans3d = builder->getTypedObject<FrameTransform3d>(transTag);
            if (theTrans3d == nullptr)
              return TCL_ERROR;
          }

          tracker.increment();
          break;

        case Position::End:
          opserr << OpenSees::PromptParseError
                 << "unexpected argument" << argv[i] << "\n";
          return TCL_ERROR;
      }
    }
  }

  if (tracker.current() != Position::End) {
      opserr << OpenSees::PromptParseError
             << "Missing required positional arguments\n";
      return TCL_ERROR;
  }

  if (ndm == 2) {
    // check plane frame problem has 3 dof per node
    if (ndf != 3) {
      opserr << OpenSees::PromptValueError << "invalid ndf: " << ndf;
      opserr << ", for plane problem need 3 \n";
      return TCL_ERROR;
    }

    if (strcmp(argv[1], "PrismFrame") == 0) {

      theBeam = new PrismFrame2d(tag, beam_data.A, beam_data.E, beam_data.Iz, 
                                 iNode, jNode, *theTrans2d,
                                 beam_data.thermal_coeff, beam_data.thermal_depth, 
                                 mass, options.mass_flag, 
                                 options.relz_flag, 
                                 options.geom_flag);
    } else {

      theBeam = new ElasticBeam2d(tag, beam_data.A, beam_data.E, beam_data.Iz, 
                                  iNode, jNode, *theTrans2d,
                                  beam_data.thermal_coeff, beam_data.thermal_depth, 
                                  mass, options.mass_flag);
    }

  } else { // ndm == 3
    //
    // Some final validation
    //
    // check space frame problem has 6 dof per node
    if ((ndf != 6 && !options.warp_flag) || (ndf != 7 && options.warp_flag)) {
      opserr << OpenSees::PromptValueError << "invalid ndof: " << ndf;
      opserr << ", for 3d problem  need 6\n";
      return TCL_ERROR;
    }

    if (theSection != nullptr) {
      // now create the beam and add it to the Domain

      std::array<int, 2> nodes {iNode, jNode};
      theBeam = new PrismFrame3d(tag, nodes, *theSection, *theTrans3d, 
                                 mass,
                                 options.mass_flag,
                                 use_mass,
                                 options.relz_flag, 
                                 options.rely_flag);

    } else {
      // now create the beam and add it to the Domain
      if (strcmp(argv[1], "PrismFrame") == 0) {
        std::array<int, 2> nodes {iNode, jNode};
        theBeam = new PrismFrame3d(tag, 
                                   nodes,
                                   beam_data.A, beam_data.E, beam_data.G, 
                                   beam_data.J, beam_data.Iy, beam_data.Iz,
                                   *theTrans3d, mass,
                                   options.mass_flag,
                                   options.relz_flag, 
                                   options.rely_flag);
                                // options.geom_flag);

      } else {
        theBeam = new ElasticBeam3d(tag, 
                                    beam_data.A, beam_data.E, beam_data.G, 
                                    beam_data.J, beam_data.Iy, beam_data.Iz,
                                    iNode, jNode,
                                    *theTrans3d, mass, options.mass_flag);
      }
    }
  }

  // now add the beam to the domain
  if (builder->getDomain()->addElement(theBeam) == false) {
    opserr << OpenSees::PromptValueError 
           << "could not add beam to domain.\n";
    opserr << *theBeam;
    delete theBeam;
    return TCL_ERROR;
  }

  return TCL_OK;
}


int
TclBasicBuilder_addElasticBeam(ClientData clientData, Tcl_Interp *interp, int argc,
                               TCL_Char ** const argv)
{
  assert(clientData != nullptr);
  BasicModelBuilder *builder = (BasicModelBuilder*)clientData;

  const int ndm = builder->getNDM();

  if (ndm == 2)
    return Parse_ElasticBeam<Args2D>(clientData, interp, argc, argv);

  else
    return Parse_ElasticBeam<Args3D>(clientData, interp, argc, argv);
}

