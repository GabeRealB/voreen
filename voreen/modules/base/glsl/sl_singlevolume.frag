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

#include "modules/mod_sampler2d.frag"
#include "modules/mod_sampler3d.frag"
#include "modules/mod_transfunc.frag"

// declare volume
uniform VolumeParameters volumeStruct_;
uniform sampler3D volume_;    // volume data set

uniform TransFuncParameters transferFunc_;
uniform TF_SAMPLER_TYPE transferFuncTex_;

in vec4 textureCoordinate[2];

vec4 renderSlice() {
    vec3 texCoord = textureCoordinate[0].xyz;

    if (inUnitCube(texCoord)) {
        float intensity = getVoxel(volume_, volumeStruct_, texCoord).a;
        vec4 color = applyTF(transferFunc_, transferFuncTex_, intensity);
        color.rgb *= color.a;
        return color;
    } else
        return vec4(0.0);
}

/***
 * The main method.
 ***/
void main() {
    FragData0 = renderSlice();
}
