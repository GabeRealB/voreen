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

#include "concretevesselgraphsource.h"

#include "voreen/core/io/serialization/jsondeserializer.h"

namespace voreen {

	ConcreteVesselGraphSource::ConcreteVesselGraphSource()
		: Processor()
		, outport_(Port::OUTPORT, "graph.output", "Graph Output", false, Processor::VALID)
		, graphFilePath_("graphFilePath", "Voreen Concrete Vessel Graph File", "Voreen Concrete Vessel Graph File", "", "*.json", FileDialogProperty::OPEN_FILE)
		, reload_("reload", "Reload Graph")
	{
		addPort(outport_);

		addProperty(graphFilePath_);
		addProperty(reload_);
	}

	ConcreteVesselGraphSource::~ConcreteVesselGraphSource() {
	}

	void ConcreteVesselGraphSource::process() {
		const std::string path = graphFilePath_.get();
		if (path.empty()) {
			return;
		}
	
		auto output = new ConcreteVesselGraph();

		JsonDeserializer deserializer;
		try {
			std::fstream f(path, std::ios::in);
			deserializer.read(f, true);
			deserializer.deserialize("graph", *output);

		}
		catch (SerializationException& e) {
			LERROR("Could not deserialize graph: " << e.what());
			outport_.setData(nullptr);
			return;
		}
		catch (...) {
			LERROR("Could not load xml file " << path);
			outport_.setData(nullptr);
			return;
		}

		LINFO("Loaded graph from file " << path << ".");
		outport_.setData(output);
	}
} // namespace voreen