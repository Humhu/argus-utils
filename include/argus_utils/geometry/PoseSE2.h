#pragma once

#include <Eigen/Geometry>
#include <iostream>

#include "sophus/se2.hpp"

#include "argus_utils/geometry/GeometryTypes.h"
#include "argus_utils/utils/LinalgTypes.h"

// TODO Wrap transform instead of rot/trans separately
// TODO Refactor
namespace argus 
{

class PoseSE3;

class PoseSE2 
{
public:

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

	static const int VectorDimension = 3;
	static const int TangentDimension = 3;
	
	typedef double ScalarType;
	typedef Eigen::Transform<ScalarType, 2, Eigen::Isometry> Transform;
	typedef Eigen::Rotation2D<ScalarType> Rotation;

	// Probability definitions
	typedef Eigen::Matrix<ScalarType, TangentDimension, TangentDimension> CovarianceMatrix;
	
	// Lie group definitions
	typedef FixedVectorType<TangentDimension> TangentVector;
	typedef FixedMatrixType<TangentDimension, TangentDimension> AdjointMatrix;
	
	/*! \brief Creates an SE2 object with 0 translation and rotation. */
	PoseSE2();

	/*! \brief Creates an SE2 object with the specified values. */
	explicit PoseSE2( double x, double y, double theta );

	/*! \brief Creates an SE2 object from a vector [x y yaw] */
	explicit PoseSE2( const VectorType& vec );

	/*! \brief Creates an SE2 object from an Eigen transform. */
	explicit PoseSE2( const Transform& trans );

	/*! \brief Creates an SE2 object from a 3x3 homogeneous matrix. */
	explicit PoseSE2( const MatrixType& m );

	/*! \brief Creates an SE2 object from a translation and rotation. */
	explicit PoseSE2( const Translation2Type& t, const Rotation& r );
	
	explicit PoseSE2( const FixedMatrixType<3,3>& H );
	
	static PoseSE2 FromSE3( const PoseSE3& se3 );

	FixedMatrixType<3,3> ToMatrix() const;
	Transform ToTransform() const;
	FixedVectorType<VectorDimension> ToVector() const;
	PoseSE2 Inverse() const;

	Translation2Type GetTranslation() const;
	PoseSE2::Rotation GetRotation() const;
	
	/*! \brief Exponentiate a tangent vector to an SE2 object. */
	static PoseSE2 Exp( const PoseSE2::TangentVector& tangent );
	
	/*! \brief Return the logarithm of an SE2 object. */
	static PoseSE2::TangentVector Log( const PoseSE2& pose );

	/*! \brief Return the adjoint matrix of an SE2 object. */
	static PoseSE2::AdjointMatrix Adjoint( const PoseSE2& pose );
	
	PoseSE2 operator*( const PoseSE2& other ) const;

protected:

	// Transform _tform;
	Sophus::SE2d _tform;

};

std::ostream& operator<<(std::ostream& os, const PoseSE2& se2);

}
