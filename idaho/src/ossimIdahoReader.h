//----------------------------------------------------------------------------
//
// License:  LGPL
// 
// See LICENSE.txt file in the top level directory for more details.
//
// Description:  Class declaration Idaho reader.
//
// Specification: ISO/IEC 15444
//
//----------------------------------------------------------------------------
// $Id: ossimIdahoReader.h 2645 2011-05-26 15:21:34Z oscar.kramer $

#ifndef ossimIdahoReader_HEADER
#define ossimIdahoReader_HEADER 1

#include <iosfwd>
#include <fstream>
#include <vector>
#include <ossim/plugin/ossimPluginConstants.h>
#include <ossim/base/ossimIpt.h>
#include <ossim/base/ossimIrect.h>
#include <ossim/imaging/ossimImageHandler.h>
#include <ossim/support_data/ossimJ2kSizRecord.h>
#include <ossim/imaging/ossimAppFixedTileCache.h>

//TODO: Idaho includes
#include <idaho/dataset.hpp>
#include <idaho/utility/reader.hpp>

// Forward class declarations.
class ossimImageData;

class ossimDpt;

class OSSIM_PLUGINS_DLL ossimIdahoReader : public ossimImageHandler {
public:

    /** default construtor */
    ossimIdahoReader();

    /** virtural destructor */
    virtual ~ossimIdahoReader();

    /**
     * @brief Returns short name.
     * @return "ossim_idaho_reader"
     */
    virtual ossimString getShortName() const;

    /**
     * @brief Returns long  name.
     * @return "ossim idaho reader"
     */
    virtual ossimString getLongName() const;

    /**
     * @brief Returns class name.
     * @return "ossimIdahoReader"
     */
    virtual ossimString getClassName() const;

    /**
     *  @brief Method to grab a tile(rectangle) from image.
     *
     *  @param rect The zero based rectangle to grab.
     *
     *  @param resLevel The reduced resolution level to grab from.
     *
     *  @return The ref pointer with the image data pointer.
     */
    virtual ossimRefPtr<ossimImageData> getTile(const ossimIrect &rect,
                                                ossim_uint32 resLevel = 0);

    virtual bool getTile(ossimImageData *result, ossim_uint32 resLevel = 0);

    /**
     *  Returns the number of bands in the image.
     *  Satisfies pure virtual from ImageHandler class.
     */
    virtual ossim_uint32 getNumberOfInputBands() const;

    /**
     * Returns the number of bands in a tile returned from this TileSource.
     * Note: we are supporting sources that can have multiple data objects.
     * If you want to know the scalar type of an object you can pass in the
     */
    virtual ossim_uint32 getNumberOfOutputBands() const;

    /**
     * Returns the tile width of the image or 0 if the image is not tiled.
     * Note: this is not the same as the ossimImageSource::getTileWidth which
     * returns the output tile width which can be different than the internal
     * image tile width on disk.
     */
    virtual ossim_uint32 getImageTileWidth() const;

    /**
     * Returns the tile width of the image or 0 if the image is not tiled.
     * Note: this is not the same as the ossimImageSource::getTileHeight which
     * returns the output tile height which can be different than the internal
     * image tile height on disk.
     */
    virtual ossim_uint32 getImageTileHeight() const;

    /**
     * Returns the output pixel type of the tile source.
     */
    virtual ossimScalarType getOutputScalarType() const;

    /**
     * @brief Gets the decimation factor for a resLevel.
     * @param resLevel Reduced resolution set for requested decimation.
     * @param result ossimDpt to initialize with requested decimation.
     */
    virtual void getDecimationFactor(ossim_uint32 resLevel,
                                     ossimDpt &result) const;

    /**
     * @brief Get array of decimations for all levels.
     * @param decimations Vector to initialize with decimations.
     */
    virtual void getDecimationFactors(vector<ossimDpt> &decimations) const;

    /**
     * @brief Returns the number of decimation levels.
     *
     * This returns the total number of decimation levels.  It is important to
     * note that res level 0 or full resolution is included in the list and has
     * decimation values 1.0, 1.0
     *
     * @return The number of decimation levels.
     */
    virtual ossim_uint32 getNumberOfDecimationLevels() const;

    /**
     * @brief Gets number of lines for res level.
     *
     *  @param resLevel Reduced resolution level to return lines of.
     *  Default = 0
     *
     *  @return The number of lines for specified reduced resolution level.
     */
    virtual ossim_uint32 getNumberOfLines(ossim_uint32 resLevel = 0) const;

    /**
     *  @brief Gets the number of samples for res level.
     *
     *  @param resLevel Reduced resolution level to return samples of.
     *  Default = 0
     *
     *  @return The number of samples for specified reduced resolution level.
     */
    virtual ossim_uint32 getNumberOfSamples(ossim_uint32 resLevel = 0) const;

    /**
     * @brief Open method.
     * @return true on success, false on error.
     */
    virtual bool open();

    /**
     *  @brief Method to test for open file stream.
     *
     *  @return true if open, false if not.
     */
    virtual bool isOpen() const;

    /**
     * @brief Method to close current entry.
     *
     * @note There is a bool kdu_compressed_source::close() and a
     * void ossimImageHandler::close(); hence, a new close to avoid conflicting
     * return types.
     */
    virtual void closeEntry();

    /**
     * Returns the image geometry object associated with this tile source or
     * NULL if non defined.  The geometry contains full-to-local image
     * transform as well as projection (image-to-world).
     */
    virtual ossimRefPtr<ossimImageGeometry> getImageGeometry();

    /**
     * Method to the load (recreate) the state of an object from a keyword
     * list.  Return true if ok or false on error.
     */
    virtual bool loadState(const ossimKeywordlist &kwl, const char *prefix = 0);

    virtual double getMinPixelValue(ossim_uint32 band = 0) const;

    virtual double getMaxPixelValue(ossim_uint32 band = 0) const;

protected:

    /**
     * @brief Gets the image geometry from external ".geom" or ".aux.xml" file.
     * @return The image geometry object associated with this idaho image.
     */
    virtual ossimRefPtr<ossimImageGeometry> getExternalImageGeometry() const;

    /**
    * @brief Method to get geometry from internal idaho tags.
    * @return The image geometry object associated with this idaho image.
    */
    virtual ossimRefPtr<ossimImageGeometry> getInternalImageGeometry() const;


private:

    bool computeDecimationFactors(std::vector<ossimDpt> &decimations) const;

    bool getImageDimensions(std::vector<ossimIrect> &tileDims) const;

    ossimRefPtr<ossimProjection> getGeoProjection();

    void getDataType();

    /**
    * @note this method assumes that setImageRectangle has been called on
    * theTile.
    */
    template<class T>
    void fillNewTile(boost::multi_array<T, 3>& data, ossimImageData *result);

    template<class T>
    void fillTileFromCache(ossimImageData *cache, ossimImageData *result);


    io::geobigdata::idaho::dataset::ptr theDataset;
    io::geobigdata::idaho::image::metadata::ptr theImageMetadata;
    io::geobigdata::idaho::image::georeferencing::ptr theGeoreferencing;
    io::geobigdata::idaho::image::histogram::ptr theImageHistogram;
    io::geobigdata::idaho::utility::reader::ptr theReader;


    ossim_uint32 theMinDwtLevels;
    ossimIrect theImageRect; /** Has sub image offset. */

    /** Image dimensions for each level. */
    std::vector<ossimIrect> theIdahoDims;

    ossim_uint32 theNumberOfBands;
    ossimScalarType theScalarType;
    ossimRefPtr<ossimImageData> theTile;
    ossimAppFixedTileCache::ossimAppFixedCacheId theCacheId;
TYPE_DATA
};

#endif /* #ifndef ossimIdahoReader_HEADER */
