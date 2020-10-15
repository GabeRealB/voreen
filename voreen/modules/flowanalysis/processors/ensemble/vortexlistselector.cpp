#include "vortexlistselector.h"

#include "voreen/core/datastructures/geometry/pointsegmentlistgeometry.h"

namespace voreen
{
	VortexListSelector::VortexListSelector() : Processor(),
		_inportVortexCollection( Port::INPORT, "inport_vortex_collection", "Vortex Collection" ),
		_outportVortexList( Port::OUTPORT, "outport_vortex_list", "Vortex List" ),
		_outportGeometry( Port::OUTPORT, "outport_geometry", "Coreline List" ),
		_propertyRuns( "property_runs", "Runs", Processor::VALID ),
		_propertyTimesteps( "property_timesteps", "Timesteps", 0, 0, std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max(), Processor::VALID ),
		_propertyCorelineLength( "property_coreline_length", "Minimal Coreline Length", 40, 2, std::numeric_limits<int>::max(), Processor::VALID ),
		_Rotation("Rotation", "Direction of Rotation:")
	{
		this->addPort( _inportVortexCollection );
		this->addPort( _outportVortexList );
		this->addPort( _outportGeometry );

		this->addProperty( _propertyRuns );
		this->addProperty( _propertyTimesteps );
		this->addProperty( _propertyCorelineLength );

		this->addProperty(_Rotation);
		_Rotation.reset();
		_Rotation.addOption("B", "Both", OPTION_B);
		_Rotation.addOption("C", "Clockwise", OPTION_CW);
		_Rotation.addOption("CCW", "Counter-Clockwise", OPTION_CCW);		
		_Rotation.selectByValue(OPTION_B);

		_inportVortexCollection.onNewData( LambdaFunctionCallback( [this]
		{
			_propertyRuns.blockCallbacks( true );
			_propertyRuns.reset();
			for( size_t i = 0; i < _inportVortexCollection.getData()->runs(); ++i )
				_propertyRuns.addRow( std::to_string( i ) );
			_propertyRuns.blockCallbacks( false );

			_propertyTimesteps.blockCallbacks( true );
			_propertyTimesteps.setMinValue( 0 );
			_propertyTimesteps.setMaxValue( static_cast<int>( _inportVortexCollection.getData()->timesteps() ) - 1 );
			_propertyTimesteps.blockCallbacks( false );

			this->updatePropertyCorelineLength();
		} ) );

		_propertyRuns.onChange( LambdaFunctionCallback( [this]
		{
			if( !_inportVortexCollection.hasData() ) return;

			this->updatePropertyCorelineLength();
			this->invalidate();
		} ) );
		_propertyTimesteps.onChange( LambdaFunctionCallback( [this]
		{
			if( !_inportVortexCollection.hasData() ) return;

			this->updatePropertyCorelineLength();
			this->invalidate();
		} ) );
		_propertyCorelineLength.onChange( LambdaFunctionCallback( [this]
		{
			this->invalidate();
		} ) );
	}

	void VortexListSelector::Process( const VortexCollection& vortexCollection, const std::vector<int>& runs, int firstTimestep, int lastTimestep, int minLength, RotationOptions rot, std::vector<Vortex>& outVortexList )
	{
		outVortexList.clear();
		for( const auto run : runs )
			for( int timestep = firstTimestep; timestep <= lastTimestep; ++timestep )
				for( const auto& vortex : vortexCollection.vortices( run, timestep ) )
					if( vortex.coreline().size() >= minLength )						
						switch(rot){
						case OPTION_B:
							outVortexList.push_back(vortex);
							break;
						case OPTION_CW:
							if (vortex.getOrientation() == Vortex::Orientation::eClockwise) { outVortexList.push_back(vortex); }//falls clockwise dreht							
							break;
						case OPTION_CCW:
							if (vortex.getOrientation() == Vortex::Orientation::eCounterClockwise) { outVortexList.push_back(vortex); }//falls counterclockwise dreht							
							break;
						default:
							break;
						}						
		outVortexList.shrink_to_fit();
	}

	void VortexListSelector::process()
	{
		const auto collection = _inportVortexCollection.getData();
		if( !collection )
		{
			_outportGeometry.clear();
			return;
		}
		
		auto outVortexList = std::unique_ptr<std::vector<Vortex>>( new std::vector<Vortex>());
		VortexListSelector::Process( *collection, _propertyRuns.get(), _propertyTimesteps.get().x, _propertyTimesteps.get().y, _propertyCorelineLength.get(), _Rotation.getValue(), *outVortexList );

		auto corelines = std::vector<std::vector<tgt::vec3>>();
		corelines.reserve(outVortexList->size());

		for( const auto& vortex : *outVortexList )
			corelines.push_back( vortex.coreline() );

		auto geometry = new PointSegmentListGeometryVec3();
		geometry->setData( std::move( corelines ) );
		_outportVortexList.setData( outVortexList.release() );
		_outportGeometry.setData( geometry );
	}

	void VortexListSelector::updatePropertyCorelineLength()
	{
		auto minLength = std::numeric_limits<int>::max();
		auto maxLength = 0;
		for( const auto run : _propertyRuns.get() )
		{
			for( auto timestep = _propertyTimesteps.get().x; timestep <= _propertyTimesteps.get().y; ++timestep )
			{
				for( const auto& vortex : _inportVortexCollection.getData()->vortices( run, timestep ) )
				{
					minLength = std::min( minLength, static_cast<int>( vortex.coreline().size() ) );
					maxLength = std::max( maxLength, static_cast<int>( vortex.coreline().size() ) );
				}
			}
		}

		_propertyCorelineLength.blockCallbacks( true );
		_propertyCorelineLength.setMinValue( minLength );
		_propertyCorelineLength.setMaxValue( maxLength );
		_propertyCorelineLength.blockCallbacks( false );
	}
}
