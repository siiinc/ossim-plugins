//----------------------------------------------------------------------------
//
// License:  See top level LICENSE.txt file
//
// Author:  Mingjie Su
//
// Description: OSSIM Idaho plugin initialization
// code.
//
//----------------------------------------------------------------------------
// $Id: ossimIdahoPluginInit.cpp 899 2010-05-17 21:00:26Z david.burken $

#include <ossim/plugin/ossimSharedObjectBridge.h>
#include <ossim/plugin/ossimPluginConstants.h>
#include <ossim/imaging/ossimImageHandlerRegistry.h>
#include <ossim/imaging/ossimImageWriterFactoryRegistry.h>
#include <ossim/imaging/ossimOverviewBuilderFactoryRegistry.h>

#include "ossimIdahoReaderFactory.h"


static void setIdahoDescription(ossimString& description)
{
   description = "Idaho reader plugin\n\n";
}

extern "C"
{
   ossimSharedObjectInfo  myIdahoInfo;
   ossimString theIdahoDescription;
   std::vector<ossimString> theIdahoObjList;

   const char* getIdahoDescription()
   {
      return theIdahoDescription.c_str();
   }

   int getIdahoNumberOfClassNames()
   {
      return (int)theIdahoObjList.size();
   }

   const char* getIdahoClassName(int idx)
   {
      if(idx < (int)theIdahoObjList.size())
      {
         return theIdahoObjList[idx].c_str();
      }
      return (const char*)0;
   }

   /* Note symbols need to be exported on windoze... */ 
   OSSIM_PLUGINS_DLL void ossimSharedLibraryInitialize(
      ossimSharedObjectInfo** info)
   {    
      myIdahoInfo.getDescription = getIdahoDescription;
      myIdahoInfo.getNumberOfClassNames = getIdahoNumberOfClassNames;
      myIdahoInfo.getClassName = getIdahoClassName;
      
      *info = &myIdahoInfo;
      
      /* Register the readers... */
      ossimImageHandlerRegistry::instance()->
         registerFactory(ossimIdahoReaderFactory::instance(), false);
      ossimIdahoReaderFactory::instance()->getTypeNameList(theIdahoObjList);
      
      setIdahoDescription(theIdahoDescription);

   }
   
   /* Note symbols need to be exported on windoze... */ 
   OSSIM_PLUGINS_DLL void ossimSharedLibraryFinalize()
   {
      ossimImageHandlerRegistry::instance()->
         unregisterFactory(ossimIdahoReaderFactory::instance());
   }
}
