/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2020 University of Muenster, Germany,                        *
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

#ifndef VRN_FLOWPARAMETERS_H
#define VRN_FLOWPARAMETERS_H

#include "voreen/core/voreencoreapi.h"

#include "voreen/core/datastructures/datainvalidationobserver.h"
#include "voreen/core/io/serialization/serializable.h"

#include "tgt/vector.h"

#include <map>

namespace voreen {

// List of flow features which can be extracted during simulation.
// Values have to be power of two (bitfield).
enum FlowFeatures {
    FF_NONE             = 0, ///< No flow feature
    FF_VELOCITY         = 1, ///< Velocity vector field
    FF_MAGNITUDE        = 2, ///< Magnitude scalar field (from velocity vector field)
    FF_PRESSURE         = 4, ///< Pressure scalar field
    FF_WALLSHEARSTRESS  = 8, ///< Wall shear stress scalar field
};

enum FlowIndicatorType {
    FIT_INVALID   = -1, ///< Denotes an invalid indicator.
    FIT_CANDIDATE =  0, ///< This indicator is just a candidate and has no function yet.
    FIT_VELOCITY  =  1, ///< This indicator is a velocity boundary condition.
    FIT_PRESSURE  =  2, ///< This indicator is a pressure boundary condition.
    FIT_MEASURE   =  3, ///< This indicator serves as a flux measure.
};

enum FlowProfile {
    FP_NONE       = 0, ///< No flow profile
    FP_POISEUILLE = 1, ///< Poiseuille flow profile
    FP_POWERLAW   = 2, ///< Power law flow profile
    FP_CONSTANT   = 3, ///< constant flow profile
};


class VelocityCurve : public Serializable {
public:

    VelocityCurve();

    float operator()(float t) const;
    float& operator[](float t);

    void setPeriodic(bool enabled);
    bool isPeriodic() const;

    static VelocityCurve createConstantCurve(float value);
    static VelocityCurve createLinearCurve(float duration, float maxValue);
    static VelocityCurve createSinusoidalCurve(float duration, float maxValue, int steps = 30);
    static VelocityCurve createHumanHeartBeat();
    static VelocityCurve createFromCSV(const std::string& file);

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

private:
    std::map<float, float> peakVelocities_;
    bool periodic_;
};

// Indicates flux through an arbitrary, circle-shaped area.
struct VRN_CORE_API FlowIndicator : public Serializable {

    FlowIndicatorType   type_;      ///< Indicator type, @see FlowIndicatorType.
    int                 id_;        ///< Unique identifier. Also used by OpenLB to indicate material.

    tgt::vec3           center_;    ///< Center position of the circle shaped area in world space.
    tgt::vec3           normal_;    ///< (Normalized) Normal vector defining the orientation.
    float               radius_;    ///< Radius of the disk.

    // Used by generating flow indicators:
    FlowProfile         flowProfile_;    ///< Flow profile, @see FlowProfile.
    VelocityCurve       velocityCurve_;  ///< Velocity curve mapping time points to velocities.

    bool                selected_;       ///< Used to indicate the currently selected indicator in UI.

    FlowIndicator();

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);
};

/**
 * Datastructure used to represent flow parameters for setting up a flow simulation. It is used in the flowsimulation module.
 */
class VRN_CORE_API FlowParameterSet : public Serializable {
public:

    /** Constructor */
    FlowParameterSet(); // For deserialization only.
    explicit FlowParameterSet(const std::string& name);

    /**
     * This function generates a unique and distinguishable name for each parametrization.
     */
    const std::string& getName() const;

    /**
     * Returns the spatial resolution in voxels, the largest vessel diameter should be divided into.
     * A high resolution is important for simulation accuracy.
     */
    int getSpatialResolution() const;
    void setSpatialResolution(int spatialResolution);

    /**
     * Returns the relaxation time parameter.
     */
     float getRelaxationTime() const;
     void setRelaxationTime(float relaxationTime);

    /**
     * Returns the max expected length in mm within the simulation geometry.
     * E.g., the largest diameter of all contained vessels.
     */
    float getCharacteristicLength() const;
    void setCharacteristicLength(float characteristicLength);

    /**
     * Returns the highest expected velocity in mm/s.
     */
    float getCharacteristicVelocity() const;
    void setCharacteristicVelocity(float characteristicVelocity);

    /**
     * Returns the kinematic viscosity in 10^-3 m^2/s.
     * Note: in order to achieve the correct physical value, multiply by 0.001
     */
    float getViscosity() const;
    void setViscosity(float viscosity);

    /**
     * Returns the fluid mass density in kg/m^3
     */
    float getDensity() const;
    void setDensity(float density);

    /**
     * Returns the constant for the Smagorinsky turbulence model.
     */
    float getSmagorinskyConstant() const;
    void setSmagorinskyConstant(float smagorinskyConstant);

    /**
     * Returns whether Bouzidi boundary condition should be used for unaligned simulation geometries.
     */
    bool getBouzidi() const;
    void setBouzidi(bool bouzidi);

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

private:

    // Identifier of the parameter set.
    std::string name_;

    // All other relevant parameters.
    int spatialResolution_;         ///< spatial resolution in voxels (per dimension and characteristic length)
    float relaxationTime_;          ///< temporal resolution in seconds
    float characteristicLength_;    ///< characteristic length in mm
    float characteristicVelocity_;  ///< characteristic velocity in mm/s
    float viscosity_;               ///< viscosity in 10^-3 m^2/s
    float density_;                 ///< density in kg/m^3
    float smagorinskyConstant_;     ///< constant for Smagorinsky turbulence model
    bool bouzidi_;                  ///< bouzidi boundary condition
};

/**
 * Parametrization List, implementing thread safety for AsyncComputeProcessor.
 */
class VRN_CORE_API FlowParameterSetEnsemble : public DataInvalidationObservable, public Serializable {

    static const int VERSION;
    static const int FLOW_INDICATOR_ID_OFFSET;

public:

    static const size_t ALL_PARAMETER_SETS;

    /**
     * Returns the offset used to generate flow indicator ids.
     * Note: the offset depends on the simulation framework.
     * E.g. OpenLB uses requires an offset of 3.
     */
    static int getFlowIndicatorIdOffset();


    explicit FlowParameterSetEnsemble(const std::string& name);
    FlowParameterSetEnsemble(const FlowParameterSetEnsemble& origin);

    const std::string& getName() const;

    /**
     * Returns the time in s which should be simulated.
     */
    float getSimulationTime() const;
    void setSimulationTime(float simulationTime);

    /**
     * Returns the number of time steps (intermediate results), the simulation should store.
     * Note: The initial time step will be stored additionally and in any case.
     */
    int getNumTimeSteps() const;
    void setNumTimeSteps(int numTimeSteps);

    /**
     * Returns the output resolution of the intermediate time steps for each volume and their dimension.
     * This enforces basically a resampling of the simulation domain.
     * Note: this currently acts as the max(!) resolution. If all the features can be captured by a lower resolution
     *       the latter will be taken.
     */
    int getOutputResolution() const;
    void setOutputResolution(int outputResolution);

    /**
     * Returns the flow features as bitmask.
     * To test for a single feature, binarily test for the available features.
     */
    int getFlowFeatures() const;
    void setFlowFeatures(int flowFeatures);

    /**
     * Add a flow indicator to the internal list.
     * Note: This will set the unique id within the parameter set ensemble.
     */
    void addFlowIndicator(const FlowIndicator& flowIndicator);
    const std::vector<FlowIndicator>& getFlowIndicators() const;

    void addFlowParameterSet(const FlowParameterSet& parameters);
    const std::vector<FlowParameterSet>& getFlowParameterSets() const;

    // Shortcuts
    bool empty() const;
    size_t size() const;
    const FlowParameterSet& at(size_t index) const;

    /** Used to export parametrization file. */
    std::string toJSONString(size_t param = ALL_PARAMETER_SETS) const;
    std::string toXMLString(size_t param = ALL_PARAMETER_SETS) const;

    void serialize(Serializer& s) const;
    void deserialize(Deserializer& s);

private:

    void serializeInternal(Serializer& s, size_t param) const;
    int generateIndicatorId() const;

    // Ensemble name.
    std::string name_;

    // Configuration.
    float simulationTime_;         ///< simulation time in seconds
    int numTimeSteps_;             ///< number of time steps of output
    int outputResolution_;         ///< spatial resolution of output in voxels (per dimension)
    int flowFeatures_;             ///< bitmask storing flow features

    // Flow indication (in-/out flow).
    std::vector<FlowIndicator> flowIndicators_;

    // Actual parameters.
    std::vector<FlowParameterSet> flowParameterSets;
};

}   // namespace

#endif
