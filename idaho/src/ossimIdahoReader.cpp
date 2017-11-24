//----------------------------------------------------------------------------
//
// License:  LGPL
// 
// See LICENSE.txt file in the top level directory for more details.
//
// Description:  Class definition for idaho reader.
//
//----------------------------------------------------------------------------
// $Id: ossimIdahoReader.cpp 2645 2011-05-26 15:21:34Z oscar.kramer $


#include "ossimIdahoReader.h"

//ossim includes
#include <ossim/base/ossimCommon.h>
#include <ossim/base/ossimDpt.h>
#include <ossim/base/ossimEndian.h>
#include <ossim/base/ossimException.h>
#include <ossim/base/ossimIoStream.h>
#include <ossim/base/ossimKeywordlist.h>
#include <ossim/base/ossimNotify.h>
#include <ossim/base/ossimScalarTypeLut.h>
#include <ossim/base/ossimTrace.h>
#include <ossim/base/ossimUnitTypeLut.h>

#include <ossim/imaging/ossimImageDataFactory.h>
#include <ossim/imaging/ossimImageGeometryRegistry.h>

#include <ossim/projection/ossimProjection.h>
#include <ossim/projection/ossimProjectionFactoryRegistry.h>
#include <ossim/projection/ossimEpsgProjectionFactory.h>
#include <ossim/base/ossim2dTo2dShiftTransform.h>

#include <ossim/support_data/ossimAuxFileHandler.h>
#include <ossim/support_data/ossimAuxXmlSupportData.h>
#include <ossim/support_data/ossimWkt.h>

//TODO: Idaho includes


// System:
#include <fstream>
#include <iostream>
#include <string>
#include <ossim/base/ossimUnitConversionTool.h>

#ifdef OSSIM_ID_ENABLED
static const char OSSIM_ID[] = "$Id";
#endif


IDAHO_USE;

static ossimTrace traceDebug("ossimIdahoReader:debug");
static ossimTrace traceDump("ossimIdahoReader:dump");
//static ossimOgcWktTranslator wktTranslator;

RTTI_DEF1_INST(ossimIdahoReader,
               "ossimIdahoReader",
               ossimImageHandler)

ossimIdahoReader::ossimIdahoReader()
        : ossimImageHandler(),
          theMinDwtLevels(0),
          theImageRect(),
          theIdahoDims(),
          theNumberOfBands(0),
          theScalarType(OSSIM_SCALAR_UNKNOWN),
          theTile(0) {
}

ossimIdahoReader::~ossimIdahoReader() {
    closeEntry();

    // Note: Not a ref ptr.  This is a  IdahoImageReader*.
//TODO: replace with real code
//    if ( theDataset )
//    {
//       theDataset->release();
//       theDataset = 0;
//    }
//    if (theImageNavigator != 0)
//    {
//       delete theImageNavigator;
//       theImageNavigator = 0;
//    }
}

ossimString ossimIdahoReader::getShortName() const {
    return ossimString("ossim_idaho_reader");
}

ossimString ossimIdahoReader::getLongName() const {
    return ossimString("ossim idaho reader");
}

ossimString ossimIdahoReader::getClassName() const {
    return ossimString("ossimIdahoReader");
}

void ossimIdahoReader::getDecimationFactor(ossim_uint32 resLevel,
                                           ossimDpt &result) const {
    if (theGeometry.valid()) {
        theGeometry->decimationFactor(resLevel, result);
    } else {
        result.makeNan();
    }
}

void ossimIdahoReader::getDecimationFactors(
        vector<ossimDpt> &decimations) const {
    if (theGeometry.valid()) {
        if (theGeometry->getNumberOfDecimations()) {
            theGeometry->decimationFactors(decimations);
        } else {
            // First time called...
            if (computeDecimationFactors(decimations)) {
                theGeometry->setDiscreteDecimation(decimations);
            } else {
                decimations.clear();
            }
        }
    } else {
        if (!computeDecimationFactors(decimations)) {
            decimations.clear();
        }
    }
}

ossim_uint32 ossimIdahoReader::getNumberOfDecimationLevels() const {
    ossim_uint32 result = 1; // Add r0

    if (theMinDwtLevels) {
        //---
        // Add internal overviews.
        //---
        result += theMinDwtLevels;
    }

    if (theOverview.valid()) {
        //---
        // Add external overviews.
        //---
        result += theOverview->getNumberOfDecimationLevels();
    }

    return result;
}

ossim_uint32 ossimIdahoReader::getNumberOfLines(
        ossim_uint32 resLevel) const {
    ossim_uint32 result = 0;
    if (isValidRLevel(resLevel)) {
        result = theImageMetadata->image_height();
    }
    return result;
}

ossim_uint32 ossimIdahoReader::getNumberOfSamples(
        ossim_uint32 resLevel) const {
    ossim_uint32 result = 0;
    if (isValidRLevel(resLevel)) {
        result = theImageMetadata->image_width();
    }
    return result;
}

bool ossimIdahoReader::open() {
    static const char MODULE[] = "ossimIdahoReader::open";

    if (traceDebug()) {
        ossimNotify(ossimNotifyLevel_DEBUG)
                << MODULE << " entered...\n"
                << "image: " << theImageFile << "\n";
    }

    bool result = false;

    if (isOpen()) {
        closeEntry();
    }

    ossimString ext = theImageFile.ext();
    ext.downcase();
    //todo: put idaho spec handling code here
    if (ext != "idaho" && !(ext.contains("graph-id") && ext.contains("node-id"))) {
        return false;
    }

    theDataset.reset(new dataset());
    bool loaded = theDataset->load(theImageFile);

    if (!loaded) {
        return false;
    }

    theReader.reset(new utility::reader(theDataset));

    theImageMetadata = theDataset->image_metadata();
    theGeoreferencing = theDataset->image_georeferencing();
    theImageHistogram = theDataset->image_histogram();

    theImageRect = ossimIrect(static_cast<ossim_int32>(theImageMetadata->minX()),
            static_cast<ossim_int32>(theImageMetadata->minY()), static_cast<ossim_int32>(theImageMetadata->maxX()),
                              static_cast<ossim_int32>(theImageMetadata->maxY()));
    theNumberOfBands = theImageMetadata->num_bands();
    theMinDwtLevels = 0;//theDataset->getNumLevels();

    getDataType();

    if (getImageDimensions(theIdahoDims)) {
        if (theScalarType != OSSIM_SCALAR_UNKNOWN) {
            uint64_t width = theImageMetadata->image_width();
            uint64_t height = theImageMetadata->image_height();

            // Check for zero width, height.
            if (!theImageRect.width() || !theImageRect.height()) {
//                ossimIpt tileSize;
//                ossim::defaultTileSize(tileSize);
//
//                width = tileSize.x;
//                height = tileSize.y;

                throw std::runtime_error("IDAHO metadata should never not have image wisth and height");
            }

            ossimIpt cacheSize(getImageTileWidth(), getImageTileHeight());

            theCacheId = ossimAppFixedTileCache::instance()->
                    newTileCache(theImageRect, cacheSize);

            theTile = ossimImageDataFactory::instance()->create(this, this);

            theTile->initialize();

            // Call the base complete open to pick up overviews.
            completeOpen();

            result = true;
        }
    }


    if (!result) {
        closeEntry();
    }

    if (traceDebug()) {
        ossimNotify(ossimNotifyLevel_DEBUG)
                << MODULE << " exit status = " << (result ? "true" : "false\n")
                << std::endl;
    }

    return result;
}

bool ossimIdahoReader::isOpen() const {
    return theTile.get() != 0;
}

void ossimIdahoReader::closeEntry() {
    theTile = 0;

    //todo: fill in cleanup

    ossimImageHandler::close();
}

ossim_uint32 ossimIdahoReader::getNumberOfInputBands() const {
    return theNumberOfBands;
}

ossim_uint32 ossimIdahoReader::getNumberOfOutputBands() const {
    return theNumberOfBands;
}

ossim_uint32 ossimIdahoReader::getImageTileWidth() const {
    return static_cast<ossim_uint32>(theImageMetadata->tile_x_size());
}

ossim_uint32 ossimIdahoReader::getImageTileHeight() const {
    return static_cast<ossim_uint32>(theImageMetadata->tile_y_size());
}

ossimScalarType ossimIdahoReader::getOutputScalarType() const {
    return theScalarType;
}

ossimRefPtr<ossimImageGeometry> ossimIdahoReader::getImageGeometry() {
    if (!theGeometry) {
        //---
        // Check for external geom:
        //---
        theGeometry = getExternalImageGeometry();

        if (!theGeometry) {
            //---
            // Check the internal geometry first to avoid a factory call.
            //---
            theGeometry = getInternalImageGeometry();

            // At this point it is assured theGeometry is set.

            // Check for set projection.
            if (!theGeometry->getProjection()) {
                // Try factories for projection.
                ossimImageGeometryRegistry::instance()->extendGeometry(this);
            }
        }

        // Set image things the geometry object should know about.
        initImageParameters(theGeometry.get());
    }

    return theGeometry;
}

ossimRefPtr<ossimImageGeometry> ossimIdahoReader::getExternalImageGeometry() const {
    // Check for external .geom file:
    ossimRefPtr<ossimImageGeometry> geom = ossimImageHandler::getExternalImageGeometry();
    if (geom.valid() == false) {
        // Check for .aux.xml file:
        ossimFilename auxXmlFile = theImageFile;
        auxXmlFile += ".aux.xml";
        if (auxXmlFile.exists()) {
            ossimAuxXmlSupportData sd;
            ossimRefPtr<ossimProjection> proj = sd.getProjection(auxXmlFile);
            if (proj.valid()) {
                geom = new ossimImageGeometry();
                geom->setProjection(proj.get());
            }
        }
    }
    return geom;
}

bool ossimIdahoReader::computeDecimationFactors(
        std::vector<ossimDpt> &decimations) const {
    bool result = true;

    decimations.clear();

    const ossim_uint32 LEVELS = getNumberOfDecimationLevels();

    for (ossim_uint32 level = 0; level < LEVELS; ++level) {
        ossimDpt pt;

        if (level == 0) {
            // Assuming r0 is full res for now.
            pt.x = 1.0;
            pt.y = 1.0;
        } else {
            // Get the sample decimation.
            ossim_float64 r0 = getNumberOfSamples(0);
            ossim_float64 rL = getNumberOfSamples(level);
            if ((r0 > 0.0) && (rL > 0.0)) {
                pt.x = rL / r0;
            } else {
                result = false;
                break;
            }

            // Get the line decimation.
            r0 = getNumberOfLines(0);
            rL = getNumberOfLines(level);
            if ((r0 > 0.0) && (rL > 0.0)) {
                pt.y = rL / r0;
            } else {
                result = false;
                break;
            }
        }

        decimations.push_back(pt);
    }

    if (traceDebug()) {
        ossimNotify(ossimNotifyLevel_DEBUG)
                << "ossimIdahoReader::computeDecimationFactors DEBUG\n";
        for (ossim_uint32 i = 0; i < decimations.size(); ++i) {
            ossimNotify(ossimNotifyLevel_DEBUG)
                    << "decimation[" << i << "]: " << decimations[i]
                    << std::endl;
        }
    }

    return result;
}

bool ossimIdahoReader::getImageDimensions(std::vector<ossimIrect> &tileDims) const {
    bool result = true;

    tileDims.clear();

    if (theDataset != 0) {
        ossimIrect imageRect(static_cast<ossim_int32>(theImageMetadata->minX()),
                             static_cast<ossim_int32>(theImageMetadata->minY()), static_cast<ossim_int32>(theImageMetadata->maxX()),
                             static_cast<ossim_int32>(theImageMetadata->maxY()));
        //ossimIrect imageRect(0, 0, theImageMetadata->image_width(), theImageMetadata->image_height());
        tileDims.push_back(imageRect);
    } else {
        result = false;
    }

    return result;
}

double ossimIdahoReader::getMinPixelValue(ossim_uint32 band) const {
    if (theImageHistogram != 0)
        return theImageHistogram->get_min(static_cast<uint16_t>(band));
}

double ossimIdahoReader::getMaxPixelValue(ossim_uint32 band) const {
    if (theImageHistogram != 0)
        return theImageHistogram->get_max(static_cast<uint16_t>(band));
}

bool ossimIdahoReader::loadState(const ossimKeywordlist &kwl,
                                 const char *prefix) {
    bool result = false;

    if (ossimImageHandler::loadState(kwl, prefix)) {
        result = open();
    }

    return result;
}

static std::map<image::datatype::tags, ossimScalarType> DATATYPE_MAP = {
        {image::datatype::tags::unknown,           OSSIM_SCALAR_UNKNOWN},
        {image::datatype::tags::_byte,             OSSIM_UINT8},
        {image::datatype::tags::_unsigned_short,   OSSIM_UINT16},
        {image::datatype::tags::_short,            OSSIM_SINT16},
        {image::datatype::tags::_unsigned_integer, OSSIM_UINT32},
        {image::datatype::tags::_integer,          OSSIM_SINT32},
        {image::datatype::tags::_float,            OSSIM_FLOAT32},
        {image::datatype::tags::_double,           OSSIM_FLOAT64}};

void ossimIdahoReader::getDataType() {
    if (theDataset != 0) {
        theScalarType = DATATYPE_MAP[theImageMetadata->data_type()];
    }
}

ossimRefPtr<ossimProjection> ossimIdahoReader::getGeoProjection() {
    if (theGeoreferencing == nullptr)
        return nullptr;

    ossimKeywordlist kwl;
    ossimString pfx = "image";
    pfx += ossimString::toString(0);
    pfx += ".";

    ossimRefPtr<ossimProjection> proj = nullptr;

    ossimString projCode = theGeoreferencing->spatial_reference_system_code();
    if (!projCode.empty()) {
        ossimString codeOnly = projCode.afterPos(4);
        kwl.add(pfx.c_str(), ossimKeywordNames::PCS_CODE_KW, codeOnly, true);
        projCode = ossimString("EPSG:") + codeOnly;
        proj = ossimEpsgProjectionFactory::instance()->createProjection(projCode);
    }

    return proj;
}

ossimGpt ComputeProjectionOrigin(ossimString proj_type, double *xform, ossim_uint32 width,
                                 ossim_uint32 height)
{
    if((proj_type == "ossimLlxyProjection") || (proj_type == "ossimEquDistCylProjection"))
    {
        ossimDpt gsd(fabs(xform[1]), fabs(xform[5]));
        ossimDpt tie(xform[0], xform[3]);
        ossimDpt halfGsd = gsd/2.0;

        //---
        // Sanity check the tie and scale for world scenes that can cause
        // a wrap issue.
        //---
        if ( (tie.x-halfGsd.x) < -180.0 )
        {
            tie.x = -180.0 + halfGsd.x;
            xform[0] = tie.x;
        }
        if ( (tie.y+halfGsd.y) > 90.0 )
        {
            tie.y = 90.0 - halfGsd.y;
            xform[3] = tie.y;
        }

        int samples = width;
        ossim_float64 degrees = samples * gsd.x;
        if ( degrees > 360.0 )
        {
            //---
            // If within one pixel of 360 degrees adjust it. Assume every
            // thing else is what they want.
            //---
            if ( fabs(degrees - 360.0) <= gsd.x )
            {
                gsd.x = 360.0 / samples;
                xform[1] = gsd.x;

                // If we adjusted scale, fix the tie if it was on the edge.
                if ( ossim::almostEqual( (tie.x-halfGsd.x), -180.0 ) == true )
                {
                    tie.x = -180 + gsd.x / 2.0;
                    xform[0] = tie.x;
                }
            }
        }
        int lines = height;
        degrees = lines * gsd.y;
        if ( degrees > 180.0 )
        {
            //---
            // If within one pixel of 180 degrees adjust it. Assume every
            // thing else is what they want.
            //---
            if ( fabs(degrees - 180.0) <= gsd.y )
            {
                gsd.y = 180.0 / lines;
                xform[5] = gsd.y;

                // If we adjusted scale, fix the tie if it was on the edge.
                if ( ossim::almostEqual( (tie.y+halfGsd.y), 90.0 ) == true )
                {
                    tie.y = 90.0 - gsd.y / 2.0;
                    xform[3] = tie.y;
                }
            }
        }

        // ESH 09/2008 -- Add the orig_lat and central_lon if the image
        // is using geographic coordsys.  This is used to convert the
        // gsd to linear units.

        // gsd could of changed above:
        halfGsd = gsd / 2.0;

        // Half the number of pixels in lon/lat directions
        int nPixelsLon = width/2.0;
        int nPixelsLat = height/2.0;

        // Shift from image corner to center in lon/lat
        double shiftLon =  nPixelsLon * fabs(gsd.x);
        double shiftLat = -nPixelsLat * fabs(gsd.y);

        // lon/lat of center pixel of the image
        double centerLon = (tie.x - halfGsd.x) + shiftLon;
        double centerLat = (tie.y + halfGsd.y) + shiftLat;

        ossimGpt(centerLat, centerLon);
    }
        return ossimGpt(0.0,0.0);
}


ossimRefPtr<ossimImageGeometry> ossimIdahoReader::getInternalImageGeometry() const {
    static const char MODULE[] = "ossimIdahoReader::getInternalImageGeometry";
    if (traceDebug()) {
        ossimNotify(ossimNotifyLevel_DEBUG) << MODULE << " entered...\n";
    }

    ossimRefPtr<ossimImageGeometry> geom = new ossimImageGeometry();

    ossimIdahoReader *th = const_cast<ossimIdahoReader *>(this);
    ossimRefPtr<ossimProjection> proj = th->getGeoProjection();

    if (ossimMapProjection *mapProj = dynamic_cast<ossimMapProjection *>(proj.get())) {
         //---
        ossimUnitType unitType = mapProj->getProjectionUnits();
        std::vector<double> xform = {theGeoreferencing->translate_x(),theGeoreferencing->scale_x(),
                                     theGeoreferencing->shear_x(),theGeoreferencing->translate_y(),
                                     theGeoreferencing->shear_y(),theGeoreferencing->scale_y()};

        if (mapProj->isGeographic()) {
            unitType = OSSIM_DEGREES;
            ossimGpt originLatLon = ComputeProjectionOrigin(mapProj->getDescription(), &xform[0], theImageRect.width(), theImageRect.height());
            ossimDpt gsd(fabs(xform[1]), fabs(xform[5]));
            ossimDpt tie(xform[0], xform[3]);
            mapProj->setUlGpt(ossimGpt(tie.y, tie.x));
            mapProj->setOrigin(originLatLon);
            mapProj->setDecimalDegreesPerPixel(gsd);
        }

        // I don't know how robust this is for everything else that says meters...
        if(unitType == OSSIM_METERS)
        {
            ossimDpt gsd(fabs(xform[1]), fabs(xform[5]));
            ossimDpt tie(xform[0], xform[3]);
            mapProj->setMetersPerPixel(gsd);
            mapProj->setUlTiePoints(tie);
        }
    }

    if (theGeoreferencing->shear_x() != 0.0 || theGeoreferencing->shear_y() != 0.0) {
        ossimNotify(ossimNotifyLevel_WARN)
                << " Unhandled rotation in IDAHO file." << std::endl;
    }
    geom->setProjection(proj.get());
    return geom;
}

ossimRefPtr<ossimImageData> ossimIdahoReader::getTile(const ossimIrect &rect, ossim_uint32 resLevel) {

    // This tile source bypassed, or invalid res level, return null tile.
    if (!isSourceEnabled() || !isOpen() || !isValidRLevel(resLevel)) {
        return ossimRefPtr<ossimImageData>();
    }

    {
        if (theTile.valid()) {
            // Image rectangle must be set prior to calling getTile.
            theTile->setImageRectangle(rect);

            if (!getTile(theTile.get(), resLevel)) {
                if (theTile->getDataObjectStatus() != OSSIM_NULL) {
                    theTile->makeBlank();
                }
            }
        }

        return theTile;
    }
}

template<typename T>
void show_array(boost::multi_array<T, 3> arr, int band, int printdims) {
    if (!traceDebug()) {
        return;
    }

    int rows = arr.shape()[0];
    int cols = arr.shape()[1];

    std::cout << "rows " << rows << std::endl;
    std::cout << "cols " << cols << std::endl;
    std::cout << "displaying matrix" << std::endl;

    for (int line = 0; line < rows && line < printdims; line++) {
        for (int sample = 0; sample < cols && sample < printdims; sample++)
            std::cout << arr[line][sample][band] << " ";
        std::cout << std::endl;
    }

    std::cout << std::endl;

}

template<typename T>
void show_buffer(ossimImageData *data, int band, int printdims) {
    if (!traceDebug()) {
        return;
    }

    ossimIpt ro = data->getOrigin();
    ossim_uint32 width = data->getWidth();
    ossim_uint32 height = data->getHeight();

    std::cout << "rows " << width << std::endl;
    std::cout << "cols " << height << std::endl;
    std::cout << "displaying matrix" << std::endl;

    // walk the data
    T *destinationBand = static_cast<T *>(data->getBuf(band));
    for (ossim_uint32 line = 0; line < height; ++line) {
        for (ossim_uint32 sample = 0; sample < width; ++sample) {
            T val = destinationBand[line * width + sample];
            T tst2 = data->getPix((ro + ossimIpt(sample, line)), band);
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

bool ossimIdahoReader::getTile(ossimImageData *result,
                               ossim_uint32 resLevel) {
    bool status = false;

    //---
    // Not open, this tile source bypassed, or invalid res level,
    // return a blank tile.
    //---
    if (isOpen() && isSourceEnabled() && isValidRLevel(resLevel) &&
        result && (result->getNumberOfBands() == getNumberOfOutputBands())) {
        result->ref();  // Increment ref count.

        status = true;

        ossimIrect tile_rect = result->getImageRectangle();

        //std::cout << std::endl << "tile_rect" << tile_rect.toString() << std::endl;

        if (getImageRectangle().intersects(tile_rect)) {
            // Make a clip rect.
            ossimIrect clip_rect = tile_rect.clipToRect(getImageRectangle());
            uint64_t tw = getImageTileWidth();
            uint64_t th = getImageTileHeight();
            clip_rect.stretchToTileBoundary(ossimIpt(static_cast<ossim_int32>(tw), static_cast<ossim_int32>(th)));
            ossim_int32 x = clip_rect.ul().x;
            ossim_int32 y = clip_rect.ul().y;

            auto tilex = static_cast<uint64_t>(std::floor((double) x / (double) tw));
            auto tiley = static_cast<uint64_t>(std::floor((double) y / (double) th));


            ossimIpt origin(x, y);

            ossimRefPtr<ossimImageData> cacheTile = ossimAppFixedTileCache::instance()->getTile(theCacheId, origin);

            if (!cacheTile.valid()) {
                if (traceDebug()) {
                    std::cout << "tilex: " << tilex << ", tiley: " << tiley << std::endl;
                }

                cacheTile = ossimImageDataFactory::instance()->create(this, this);
                cacheTile->setImageRectangle(clip_rect);
                cacheTile->initialize();

                client::tile_proxy::ptr tp = theDataset->get_tile_proxy(tilex, tiley);
                theDataset->get(tp);
                //get native type array
                switch (getOutputScalarType()) {

                    case OSSIM_UINT8: {
                        utility::image_array<uint8_t> image;
                        boost::multi_array<uint8_t, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<uint8_t>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_UINT16: {
                        utility::image_array<uint16_t> image;
                        boost::multi_array<uint16_t, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<ossim_uint16>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_SINT16: {
                        utility::image_array<int16_t> image;
                        boost::multi_array<int16_t, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<int16_t>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_UINT32: {
                        utility::image_array<uint32_t> image;
                        boost::multi_array<uint32_t, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<uint32_t>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_SINT32: {
                        utility::image_array<int32_t> image;
                        boost::multi_array<int32_t, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<int32_t>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_FLOAT32: {
                        utility::image_array<float> image;
                        boost::multi_array<float, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<float>(arr, cacheTile.get());
                    }
                        break;
                    case OSSIM_FLOAT64: {
                        utility::image_array<double> image;
                        boost::multi_array<double, 3> arr = image.get_array(theReader, tp);
                        fillNewTile<double>(arr, cacheTile.get());
                    }
                        break;
                    default:
                        throw std::runtime_error("Unhandled datatype");
                }

                cacheTile->validate();
                ossimAppFixedTileCache::instance()->addTile(theCacheId, cacheTile.get(), false);
            }

            switch (getOutputScalarType()) {
                case OSSIM_UINT8: {
                    fillTileFromCache<uint8_t>(cacheTile.get(), result);
                }
                    break;
                case OSSIM_UINT16: {
                    fillTileFromCache<uint16_t>(cacheTile.get(), result);
                }
                    break;
                case OSSIM_SINT16: {
                    fillTileFromCache<int16_t>(cacheTile.get(), result);
                }
                    break;
                case OSSIM_UINT32: {
                    fillTileFromCache<uint32_t>(cacheTile.get(), result);
                }
                    break;
                case OSSIM_SINT32: {
                    fillTileFromCache<int32_t >(cacheTile.get(), result);
                }
                    break;
                case OSSIM_FLOAT32: {
                    fillTileFromCache<float>(cacheTile.get(), result);
                }
                    break;
                case OSSIM_FLOAT64: {
                    fillTileFromCache<double>(cacheTile.get(), result);
                }
                    break;
                default:
                    throw std::runtime_error("Unhandled datatype");
            }

        } else // No intersection...
        {
            result->makeBlank();
        }

        result->unref();  // Decrement ref count.
    }

    return status;
}

template<class T>
void ossimIdahoReader::fillNewTile(boost::multi_array<T, 3> &data, ossimImageData *result) {

    const ossimIrect img_rect = result->getImageRectangle();
    // Check the status and allocate memory if needed.
    if (result->getDataObjectStatus() == OSSIM_NULL) {
        result->initialize();
    }

    // Get the width of the buffers.
    ossim_uint32 num_bands = result->getNumberOfBands();

    ossimIpt ro = result->getOrigin();
    ossim_uint32 width = result->getWidth();
    ossim_uint32 height = result->getHeight();


    // Copy the data.
    for (ossim_uint32 band = 0; band < num_bands; band++) {
        T *destinationBand = static_cast<T *>(result->getBuf(band));
        for (ossim_uint32 line = 0; line < height; ++line) {
            for (ossim_uint32 sample = 0; sample < width; ++sample) {
                T val = data[line][sample][band];
                destinationBand[line * width + sample] = val;

                if (traceDebug()) {
                    T tst = destinationBand[line * width + sample];
                    T tst2 = result->getPix((ro + ossimIpt(sample, line)), band);
                    if (tst != tst2)
                        throw std::runtime_error("debug error trigger");
                }
            }
        }
    }

    result->validate();
}


template<class T>
void ossimIdahoReader::fillTileFromCache(ossimImageData *cache,
                                         ossimImageData *result) {

    if (result->getDataObjectStatus() == OSSIM_NULL) {
        result->initialize();
    }

    // Get the width of the buffers.
    ossim_uint32 num_bands = result->getNumberOfBands();
    ossimIpt co = cache->getOrigin();
    ossimIpt ro = result->getOrigin();

    ossim_uint32 rStartX = 0;
    ossim_uint32 rWidth = result->getWidth();
    ossim_uint32 rStartY = 0;
    ossim_uint32 rHeight = result->getHeight();


    ossim_int32 srcOffsetX = ro.x - co.x;
    ossim_uint32 cWidth = cache->getWidth();
    ossim_int32 srcOffsetY = ro.y - co.y;
    ossim_uint32 cHeight = cache->getHeight();

    if (traceDebug()) {

        std::cout << "srcOffsetX=" << srcOffsetX << std::endl;
        std::cout << "srcOffsetY=" << srcOffsetY << std::endl;
        std::cout << "rStartX=" << rStartX << ", rWidth=" << rWidth << std::endl;
        std::cout << "rStartY=" << rStartY << ", rHeight=" << rHeight << std::endl;
    }

    //make sure rects completely overlap
    if (!result->getImageRectangle().completely_within(cache->getImageRectangle()))
        throw std::runtime_error("rects don't overlap... this shouldn't happen");

    // Copy the data.
    for (ossim_uint32 band = 0; band < num_bands; band++) {
        auto *inBufPtr = static_cast<T *>(cache->getBuf(band));
        auto *outBufPtr = static_cast<T *>(result->getBuf(band));
        for (ossim_uint32 line = rStartY; line < rHeight; ++line) {
            for (ossim_uint32 sample = rStartX; sample < rWidth; ++sample) {
                ossim_uint32 srcX = sample + srcOffsetX;
                ossim_uint32 srcY = line + srcOffsetY;
                ossim_uint64 srcIdx = srcY * cWidth + srcX;

                ossim_uint64 dstIdx = line * rWidth + sample;
                T val = inBufPtr[srcIdx];
                if (traceDebug()) {
                    T tst3 = cache->getPix((co + ossimIpt(srcX, srcY)), band);
                    if (val != tst3)
                        throw std::runtime_error("debug error trigger");
                }
                outBufPtr[dstIdx] = val;
            }
        }
    }

    result->validate();
}
