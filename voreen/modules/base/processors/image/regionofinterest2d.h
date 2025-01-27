/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#ifndef VRN_REGIONOFINTEREST_2D_H
#define VRN_REGIONOFINTEREST_2D_H

#include "voreen/core/processors/imageprocessor.h"
#include "voreen/core/properties/eventproperty.h"
#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/matrixproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/filedialogproperty.h"
#include "voreen/core/properties/vectorproperty.h"
#include "voreen/core/datastructures/geometry/geometry.h"
#include "voreen/core/io/serialization/serialization.h"
#include "voreen/core/interaction/idmanager.h"
#include "voreen/core/properties/colorproperty.h"

namespace voreen {

/**
 * Allows the user to sketch arbitrarily shaped 2D regions of interest: points, line strip, spline curve, free-form patch.
 *
 * The ROIs are put out as mask and additionally as overlay drawn over the input rendering.
 * Furthermore, the RegionOfInterest2D processor is designed to be used in combination with
 * a SliceViewer: The picking matrix generated by the SliceViewer can be linked with
 * RegionOfInterest2D's matrix property in order to convert screen coordinates into
 * volume coordinates.
 *
 * @see SliceViewer
 */
class VRN_CORE_API RegionOfInterest2D : public ImageProcessor {

public:
    RegionOfInterest2D();
    Processor* create() const { return new RegionOfInterest2D; }

    std::string getClassName() const { return "RegionOfInterest2D"; }
    std::string getCategory() const  { return "Image Processing"; }
    CodeState getCodeState() const   { return CODE_STATE_STABLE; }

    bool isReady() const;

    void saveROIs(const std::string& filename) const;
    void loadROIs(const std::string& filename);

    /**
     * Geometry representing one region of interest (ROI).
     */
    class RegionOfInterestGeometry : public Geometry {

    public:

        /// Determines how the set of ROI points specified by the user is interpreted.
        enum GeometryMode {
            POINTS,         ///< ROI points are rendered as GL_POINTS.
            LINES,          ///< ROI points are rendered as GL_LINES.
            LINE_STRIP,     ///< ROI points are rendered as GL_LINE_STRIP.
            LINE_LOOP,      ///< ROI points are rendered as GL_LINE_LOOP.
      //      SPLINE,         ///< ROI points are rendered as a GLU NURBS curve.
      //      PATCH,          ///< ROI points are treated as concav polygon that is tesselated
                            ///    by the GLU tesselation routine.
      //      CIRCLE_PATCH,   ///< ROI is a circle, approximated by a patch
            RECTANGLE       ///< ROI is a rectangle defined by two points, rendered as GL_QUAD
        };

        /// Standard constructor.
        RegionOfInterestGeometry();

        virtual ~RegionOfInterestGeometry();

        virtual Geometry* create() const;

        virtual std::string getClassName() const { return "RegionOfInterestGeometry"; }

        /// @see Geometry::render
        void render() const;

        void renderBoundingBox() const;

        void computeBoundingBox();

        void fillArea(tgt::Texture* mask);

        /// @see Serializer::serialize
        void serialize(Serializer& s) const;

        /// @see Deserializer::deserialize
        void deserialize(Deserializer& s);

        std::vector<tgt::vec2> points_;         ///< The points defining the ROI, specified in viewport coordinates
        std::vector<tgt::vec2> fillPoints_;     ///< The points defining the ROI including the inner area, specified in viewport coordinates
        int layer_;                             ///< The layer the ROI is associated with.
        tgt::ivec2 viewportSize_;               ///< The viewport dimensions when the ROI was drawn.
        tgt::mat4 transformMatrix_;             ///< Viewport-to-VolumeCoords transformation when the ROI was drawn

        GeometryMode geometryMode_;             ///< Determines the geometry type of the ROI
        tgt::Color roiColor_;                   ///< The color of the ROI overlay (drawn onto the input image)
        tgt::Color maskColor_;                  ///< The mask color of the ROI (for mask image)
        float size_;                            ///< Point size or line width, respectively, if in line or point mode.
        bool antialiasing_;                     ///< Use OpenGL line/point smooth, respectively?

        tgt::vec4 boundingBox_  ;               ///< ROI points' bounding box (ll.x, ll.y, ur.x, ur.y)

        static const std::string loggerCat_;
    };

protected:
    virtual void setDescriptions() {
        setDescription("<p>Allows the user to sketch arbitrarily shaped 2D regions of interest: points, line strip, spline curve, free-form patch. The ROIs are put out as mask and additionally as overlay drawn over the input rendering. Multiple ROIs can be defined consecutively. Each of them is assigned the properties that are set when the user starts drawing the ROI. Double-click clears the current ROI set.<p/>\
<p>The RegionOfInterest2D processor is designed to be used in combination with a SliceViewer: The picking matrix generated by the SliceViewer can be linked with RegionOfInterest2D's matrix property in order to convert screen coordinates into volume coordinates.</p>");
    }

    enum RoiMode {
        COLOR_MODE,
        MASK_MODE,
        PICKING_MODE
    };

    void initialize();
    void deinitialize();
    void process();

    /**
     * Renders the stored ROIs to the currently active output target.
     *
     * @param maskMode if true, the ROIs are rendered with the global mask color,
     *      otherwise, they are rendered with their roi color.
     */
    void renderROIs(RoiMode roiMode);

    void renderBoundingBox(RegionOfInterestGeometry* roi) const;

    void addROIEvent(tgt::MouseEvent* e);
    void removeLastROI(tgt::MouseEvent* e = 0);
    void removeROIInCurrentSlice(tgt::MouseEvent* e = 0);
    void removeClosestROIInCurrentSlice(tgt::MouseEvent* e = 0);
    void clearROIs(tgt::MouseEvent* e = 0);
    void shiftROI(tgt::MouseEvent* e);
    void scaleROI(tgt::MouseEvent* e);

    std::vector<tgt::vec2> generateCircle(tgt::vec2 center, float radius) const;
    std::vector<tgt::vec2> generateRectangle(tgt::vec2 first, tgt::vec2 second) const;

    void serialize(Serializer& s) const;
    void deserialize(Deserializer& s);

    RenderPort inport_;
    RenderPort outportMask_;
    RenderPort maskPrivate_;
    RenderPort outportROI_;
    RenderPort outportOverlay_;
    RenderPort pickingPort_;
    GeometryPort outportROIGeometryLayer_;
    GeometryPort outportROIGeometry_;

    OptionProperty<RegionOfInterestGeometry::GeometryMode> geometryMode_;
    ColorProperty roiColor_;
    ColorProperty maskColor_;
    ColorProperty maskBackgroundColor_;
    FloatProperty size_;
    BoolProperty antialiasing_;
    IntProperty currentLayer_;
    BoolProperty renderCurrentLayerOnly_;
    ColorProperty boundingBoxColor_;
    FloatProperty minRoiPointDistance_;
    BoolProperty fillRegions_;
    FloatMat4Property roiTransformMatrix_;
    FileDialogProperty loadROIs_;
    FileDialogProperty saveROIs_;
    ButtonProperty clearROIs_;
    ButtonProperty removeLastROI_;
    ButtonProperty removeLastROIInCurSlice_;
    IntProperty numberOfROIs_;

    EventProperty<RegionOfInterest2D> mouseEventAddROI_;
    EventProperty<RegionOfInterest2D> mouseEventRemoveLastROI_;
    EventProperty<RegionOfInterest2D> mouseEventRemoveLastROIInCurSlice_;
    EventProperty<RegionOfInterest2D> mouseEventRemoveClosestROIInCurSlice_;
    EventProperty<RegionOfInterest2D> mouseEventClearROIs_;
    EventProperty<RegionOfInterest2D> mouseEventShiftROI_;
    EventProperty<RegionOfInterest2D> mouseEventScaleROI_;

    tgt::Shader* blendShader_;
    IDManager idManager_;

    /// The user-defined rois
    std::vector<RegionOfInterestGeometry*> regionsOfInterest_;

    /// The roi currently selected by the user
    int selectedROI_;

    tgt::ivec2 lastMousePos_;
    bool geometryHasChanged_;

private:
    void saveFileIssued();
    void loadFileIssued();
    void updateGeometry();
    void renderROIMask(RegionOfInterestGeometry* roi);
};

} // namespace voreen

#endif //VRN_REGIONOFINTEREST_2D_H
