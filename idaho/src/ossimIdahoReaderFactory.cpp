//----------------------------------------------------------------------------
//
// License:  See top level LICENSE.txt file
//
// Author:  Mingjie Su
//
// Description: Factory for OSSIM Idaho reader using kakadu library.
//----------------------------------------------------------------------------
// $Id: ossimIdahoReaderFactory.cpp 469 2009-12-23 18:52:47Z ming.su $

#include <ossim/base/ossimKeywordlist.h>
#include <ossim/base/ossimRefPtr.h>
#include <ossim/base/ossimString.h>
#include <ossim/base/ossimTrace.h>
#include <ossim/imaging/ossimImageHandler.h>

#include "ossimIdahoReaderFactory.h"
#include "ossimIdahoReader.h"

static const ossimTrace traceDebug("ossimIdahoReaderFactory:debug");

class ossimImageHandler;

RTTI_DEF1(ossimIdahoReaderFactory,
          "ossimIdahoReaderFactory",
          ossimImageHandlerFactoryBase);

ossimIdahoReaderFactory* ossimIdahoReaderFactory::theInstance = 0;

ossimIdahoReaderFactory::~ossimIdahoReaderFactory()
{
   theInstance = 0;
}

ossimIdahoReaderFactory* ossimIdahoReaderFactory::instance()
{
   if(!theInstance)
   {
      theInstance = new ossimIdahoReaderFactory;
   }
   return theInstance;
}

ossimImageHandler* ossimIdahoReaderFactory::open(
   const ossimFilename& fileName, bool openOverview)const
{
   if(traceDebug())
   {
      ossimNotify(ossimNotifyLevel_DEBUG)
         << "ossimIdahoReaderFactory::open(filename) DEBUG: entered...\n";
   }

   ossimRefPtr<ossimImageHandler> reader = 0;

   if ( hasExcludedExtension(fileName) == false )
   {
      if(traceDebug())
      {
         ossimNotify(ossimNotifyLevel_DEBUG) << "\nTrying ossimIdahoReader\n";
      }
      reader = new ossimIdahoReader;
      reader->setOpenOverviewFlag(openOverview);
      if(reader->open(fileName) == false)
      {
         reader = 0;
      }
   }

   if(traceDebug())
   {
      ossimNotify(ossimNotifyLevel_DEBUG)
         << "ossimIdahoReaderFactory::open(filename) DEBUG: leaving...\n";
   }

   return reader.release();
}

ossimImageHandler* ossimIdahoReaderFactory::open(const ossimKeywordlist& kwl,
                                                 const char* prefix)const
{
   if(traceDebug())
   {
      ossimNotify(ossimNotifyLevel_DEBUG)
         << "ossimIdahoReaderFactory::open(kwl, prefix) DEBUG: entered..."
         << "Trying ossimIdahoReader..."
         << std::endl;
   }

   ossimRefPtr<ossimImageHandler> reader = new ossimIdahoReader;
   if(reader->loadState(kwl, prefix) == false)
   {
      reader = 0;
   }

   if(traceDebug())
   {
      ossimNotify(ossimNotifyLevel_DEBUG)
         << "ossimIdahoReaderFactory::open(kwl, prefix) DEBUG: leaving..."
         << std::endl;
   }

   return reader.release();
}

ossimObject* ossimIdahoReaderFactory::createObject(
   const ossimString& typeName)const
{
   ossimRefPtr<ossimObject> result = 0;
   if(typeName == "ossimIdahoReader")
   {
      result = new ossimIdahoReader;
   }
   return result.release();
}

ossimObject* ossimIdahoReaderFactory::createObject(
   const ossimKeywordlist& kwl,
   const char* prefix)const
{
   return this->open(kwl, prefix);
}

void ossimIdahoReaderFactory::getTypeNameList(
   std::vector<ossimString>& typeList)const
{
   typeList.push_back(ossimString("ossimIdahoReader"));
}

void ossimIdahoReaderFactory::getSupportedExtensions(
   ossimImageHandlerFactoryBase::UniqueStringList& extensionList)const
{
   extensionList.push_back(ossimString("idaho"));
}

void ossimIdahoReaderFactory::getImageHandlersBySuffix(
   ossimImageHandlerFactoryBase::ImageHandlerList& result, const ossimString& ext)const
{
   ossimString testExt = ext.downcase();
   if(ext == "idaho" || !(ext.contains("graph-id") && ext.contains("node-id")))
   {
      result.push_back(new ossimIdahoReader);
   }
}

void ossimIdahoReaderFactory::getImageHandlersByMimeType(
   ossimImageHandlerFactoryBase::ImageHandlerList& result, const ossimString& mimeType)const
{
   ossimString testExt = mimeType.downcase();
   if(testExt == "image/idaho")
   {
      result.push_back(new ossimIdahoReader);
   }
}

bool ossimIdahoReaderFactory::hasExcludedExtension(
   const ossimFilename& file) const
{
   bool result = true;
   ossimString ext = file.ext().downcase();
   if (ext == "idaho") //only include the file with .idaho extension and exclude any other files
   {
      result = false;
   }
   return result;
}

ossimIdahoReaderFactory::ossimIdahoReaderFactory(){}

ossimIdahoReaderFactory::ossimIdahoReaderFactory(const ossimIdahoReaderFactory&){}

void ossimIdahoReaderFactory::operator=(const ossimIdahoReaderFactory&){}
