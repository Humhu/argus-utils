#include "argus_utils/geometry/PoseSE2.h"
#include "argus_utils/geometry/PoseSE3.h"
#include <cmath>

namespace argus
{

PoseSE2::PoseSE2() {}

PoseSE2::PoseSE2( const double x, const double y, const double theta ) 
{
	Sophus::SE2d::Point trans( x, y );
	_tform = Sophus::SE2d( theta, trans );
}

PoseSE2::PoseSE2( const VectorType &vec ) 
{
	if( vec.size() != 3 )
	{
		throw std::runtime_error( "PoseSE2 must be constructed from 3-element vector." );
	}
	_tform = Sophus::SE2d( vec(2), Sophus::SE2d::Point( vec(0), vec(1) ) );
}

PoseSE2::PoseSE2( const MatrixType& m ) 
{
	if( m.cols() != 3 || m.rows() != 3 )
	{
		throw std::runtime_error( "PoseSE2 must be constructed from 3x3 matrix.");
	}
	
	_tform = Sophus::SE2d( m.topLeftCorner<2,2>(),
	                       Sophus::SE2d::Point( m(0,2), m(1,2) ) );
}

PoseSE2::PoseSE2( const FixedMatrixType<3,3>& H )
{
	_tform = Sophus::SE2d( H.topLeftCorner<2,2>(),
	                       Sophus::SE2d::Point( H(0,2), H(1,2) ) );
}

PoseSE2::PoseSE2( const Translation2Type& t, const Rotation& r ) 
{
	_tform = Sophus::SE2d( r.matrix(), Sophus::SE2d::Point( t.x(), t.y() ) );
}

PoseSE2 PoseSE2::FromSE3( const PoseSE3& se3 )
{
	FixedMatrixType<4,4> H = se3.ToMatrix();
	FixedMatrixType<3,3> H2 = FixedMatrixType<3,3>::Identity();
	H2.topLeftCorner<2,2>() = H.topLeftCorner<2,2>();
	H2.topRightCorner<2,1>() = H.topRightCorner<2,1>();
	return PoseSE2( H2 );
}

PoseSE2::TangentVector PoseSE2::Log( const PoseSE2& pose ) 
{
	return Sophus::SE2d::log( pose._tform );
}

PoseSE2 PoseSE2::Exp( const PoseSE2::TangentVector& tangent ) 
{
	PoseSE2 out;
	out._tform = Sophus::SE2d::exp( tangent );
	return out;
}

PoseSE2::AdjointMatrix PoseSE2::Adjoint( const PoseSE2& pose ) 
{
	return pose._tform.Adj();
}

FixedMatrixType<3,3> PoseSE2::ToMatrix() const
{
	return _tform.matrix();
}

Translation2Type PoseSE2::GetTranslation() const 
{
	
	Translation2Type out( _tform.translation()(0),
	                      _tform.translation()(1) );
	return out;
}

PoseSE2::Rotation PoseSE2::GetRotation() const 
{
	Rotation rot( 0 );
	rot.fromRotationMatrix( _tform.so2().matrix() );
	return rot;
}

PoseSE2::Transform PoseSE2::ToTransform() const 
{
	return Transform( _tform.matrix() );
}

FixedVectorType<PoseSE2::VectorDimension> 
PoseSE2::ToVector() const 
{
	FixedVectorType<PoseSE2::VectorDimension> ret;
	ret << _tform.translation()(0), _tform.translation()(1), Sophus::SO2d::log( _tform.so2() );
	return ret;
}

PoseSE2 PoseSE2::Inverse() const 
{
	PoseSE2 ret;
	ret._tform = _tform.inverse();
	return ret;
}

PoseSE2 PoseSE2::operator*(const PoseSE2& other) const 
{
	PoseSE2 ret;
	ret._tform = _tform * other._tform;
	return ret;
}

std::ostream& operator<<(std::ostream& os, const PoseSE2& se2) 
{
	VectorType vec = se2.ToVector();
	os << vec(0) << " " << vec(1) << " " << vec(2);
	return os;
}

}
