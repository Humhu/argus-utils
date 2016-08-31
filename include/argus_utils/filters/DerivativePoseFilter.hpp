#pragma once

#include "argus_utils/utils/LinalgTypes.h"
#include "argus_utils/filters/FilterInfo.h"
#include "argus_utils/random/MultivariateGaussian.hpp"
#include <Eigen/Cholesky>
#include <boost/bind.hpp>
#include <iostream>
namespace argus
{

/*! \brief Construct a discrete-time integrator matrix with the
 * specified integration order. 0th order corresponds to no integration.
 * A negative order input will produce the max integration order NumDerivs.*/
template <int VecDim, int NumDerivs>
FixedMatrixType<VecDim*(NumDerivs+1), VecDim*(NumDerivs+1)>
IntegralMatrix( double dt, int order )
{
	typedef FixedMatrixType<VecDim*(NumDerivs+1), VecDim*(NumDerivs+1)> MatType;
	MatType mat = MatType::Identity();

	if( order < 0 ) { order = NumDerivs; }
	double intTerm = 1.0;
	for( int o = 1; o <= order; ++o )
	{
		intTerm = intTerm * dt / o;
		for( int i = 0; i < VecDim*(NumDerivs-o+1); ++i )
		{
			mat( i, i + VecDim*o ) = intTerm;
		}
	}
	return mat;
}

/*! \brief A Kalman filter that tracks pose and N derivatives. */
template <typename P, int N = 1>
class DerivativePoseFilter
{
public:
	
	typedef P PoseType;
	static const int TangentDim = PoseType::TangentDimension;
	static const int DerivsDim = N * TangentDim;
	static const int CovarianceDim = (N+1) * TangentDim;
	
	typedef typename PoseType::TangentVector TangentType;
	typedef FixedVectorType<DerivsDim> DerivsType;
	typedef FixedVectorType<CovarianceDim> FullVecType;

	typedef FixedMatrixType<TangentDim,TangentDim> PoseCovType;
	typedef FixedMatrixType<DerivsDim,DerivsDim> DerivsCovType;
	typedef FixedMatrixType<CovarianceDim,CovarianceDim> FullCovType;

	typedef WidthFixedMatrixType<DerivsDim> DerivObsMatrix;
	typedef WidthFixedMatrixType<CovarianceDim> FullObsMatrix;

	typedef FixedMatrixType<TangentDim,CovarianceDim> PoseObsMatrix;
	typedef FixedMatrixType<TangentDim,TangentDim> PoseObsCovType;

	// Takes delta time as input and returns transition matrix
	typedef FixedMatrixType<CovarianceDim,CovarianceDim> TransitionMatrix;
	typedef boost::function <TransitionMatrix (double)> TransMatFunc;

	
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	DerivativePoseFilter() 
	: _pose(), 
	  _derivs( DerivsType::Zero() ), 
	  _cov( FullCovType::Identity() ),
	  _normal( TangentDim ) 
	{
		_tfunc = boost::bind( &IntegralMatrix<TangentDim,N>, _1, -1 );
	}

	DerivativePoseFilter( const PoseType& pose, const DerivsType& derivs,
	                      const FullCovType& cov )
	: _pose( pose ), _derivs( derivs ), _cov( cov ), _normal( TangentDim )
	{
		_tfunc = boost::bind( &IntegralMatrix<TangentDim,N>, _1, -1 );
	}

	TransMatFunc& TransitionMatrixFunc() { return _tfunc; }
	PoseType& Pose() { return _pose; }
	const PoseType& Pose() const { return _pose; }
	DerivsType& Derivs() { return _derivs; }
	const DerivsType& Derivs() const { return _derivs; }
	Eigen::Block<FullCovType> PoseCov() 
	{
		return _cov.topLeftCorner(TangentDim,TangentDim); 
	}
	Eigen::Block<const FullCovType> PoseCov() const 
	{
		return _cov.topLeftCorner(TangentDim,TangentDim); 
	}
	Eigen::Block<FullCovType> DerivsCov() 
	{
		return _cov.bottomRightCorner(DerivsDim,DerivsDim);
	}
	Eigen::Block<const FullCovType> DerivsCov() const 
	{
		return _cov.bottomRightCorner(DerivsDim,DerivsDim);
	}
	FullCovType& FullCov() { return _cov; }
	const FullCovType& FullCov() const { return _cov; }

	/*! \brief Displace the pose mean by a world displacement (left multiply). */
	void WorldDisplace( const PoseType& d, const PoseCovType& Q )
	{
		typename PoseType::AdjointMatrix adj = PoseType::Adjoint( _pose );
		_pose = d * _pose;
		_cov.template topLeftCorner<TangentDim,TangentDim>() =
			adj * _cov.template topLeftCorner<TangentDim,TangentDim>() * adj.transpose() + Q;
	}

	/*! \brief Perform a predict step where the derivatives are used
	 * to integrate the forward pose. */
	PredictInfo Predict( const FullCovType& Q, double dt )
	{
		FullVecType x;
		x.template head<TangentDim>() = FixedVectorType<TangentDim>::Zero();
		x.template tail<DerivsDim>() = _derivs;
		
		PredictInfo info;
		info.xpre = x;
		info.Spre = _cov;
		info.dt = dt;
		info.Q = Q;

		// We use A first without the adjoint in the upper left block, but that 
		// block does not affect the derivatives anyways
		TransitionMatrix A = _tfunc( dt );
		FullVecType xup = A * x;
		PoseType displacement = PoseType::Exp( xup.template segment<TangentDim>(TangentDim) * dt );
		A.template topLeftCorner<TangentDim,TangentDim>() = PoseType::Adjoint( displacement );
		_derivs = xup.template tail<DerivsDim>();
		_pose = _pose * displacement;
		_cov = A*_cov*A.transpose() + Q;

		info.F = A;
		return info;
	}

	double DerivsLikelihood( const VectorType& obs, const DerivObsMatrix& C,
	                         const MatrixType& R )
	{
		unsigned int zDim = obs.size();
		if( zDim != C.rows() ) 
		{
			throw std::runtime_error( "DerivativePoseFilter: Deriv update dimension mismatch." );
		}

		VectorType v = obs - C*_derivs;

		FullObsMatrix Cfull( zDim, CovarianceDim );
		Cfull.template leftCols<TangentDim>() = WidthFixedMatrixType<TangentDim>::Zero( TangentDim, zDim );
		Cfull.template rightCols<DerivsDim>() = C;


		MatrixType V = Cfull*_cov*Cfull.transpose() + R;
		return GaussianPDF( V, v );
	}

	UpdateInfo UpdateDerivs( const VectorType& obs, const DerivObsMatrix& C,
	                         const MatrixType& R )
	{
		unsigned int zDim = obs.size();
		if( zDim != C.rows() ) 
		{
			throw std::runtime_error( "DerivativePoseFilter: Deriv update dimension mismatch." );
		}

		VectorType v = obs - C*_derivs;

		FullObsMatrix Cfull( zDim, CovarianceDim );
		Cfull.template leftCols<TangentDim>() = WidthFixedMatrixType<TangentDim>::Zero( TangentDim, zDim );
		Cfull.template rightCols<DerivsDim>() = C;

		MatrixType V = Cfull*_cov*Cfull.transpose() + R;
		Eigen::LDLT<MatrixType> Vinv( V );
		MatrixType K = _cov * Cfull.transpose() * Vinv.solve( MatrixType::Identity( V.rows(), V.cols() ) );
		
		// FullVecType correction = _cov * Cfull.transpose() * Vinv.solve( v );
		FullVecType correction = K * v;
		TangentType poseCorrection = correction.template head<TangentDim>();
		DerivsType derivsCorrection = correction.template tail<DerivsDim>();

		// TODO
		FullVecType x;
		x.template head<TangentDim>() = FixedVectorType<TangentDim>::Zero();
		x.template tail<DerivsDim>() = _derivs;
		UpdateInfo info;
		info.xpre = x;
		info.Spre = _cov;

		_pose = _pose * PoseType::Exp( poseCorrection );
		_derivs = _derivs + derivsCorrection;
		// Joseph form more stable?
		// _cov = _cov - _cov * Cfull.transpose() * Vinv.solve( Cfull * _cov );
		MatrixType l = FullCovType::Identity() - K*Cfull;
		_cov = l * _cov * l.transpose() + K * R * K.transpose();

		x.template head<TangentDim>() = FixedVectorType<TangentDim>::Zero();
		x.template tail<DerivsDim>() = _derivs;
		info.observation = obs;
		info.innovation = v;
		info.post_innovation = obs - C*_derivs;
		info.delta_x = x - info.xpre;
		info.Spost = _cov;
		info.H = Cfull;
		info.R = R;

		return info;
	}

	UpdateInfo UpdatePose( const PoseType& obs, const PoseObsCovType& R )
	{
		PoseObsMatrix Cfull;
		Cfull.template leftCols<TangentDim>() = FixedMatrixType<TangentDim,TangentDim>::Identity();
		Cfull.template rightCols<DerivsDim>() = FixedMatrixType<TangentDim,DerivsDim>::Zero();

		PoseType err = _pose.Inverse() * obs;
		typename PoseType::TangentVector v = PoseType::Log( err );
		MatrixType V = Cfull*_cov*Cfull.transpose() + R;
		Eigen::LDLT<MatrixType> Vinv( V );
		
		FullVecType correction = _cov * Cfull.transpose() * Vinv.solve( v );
		TangentType poseCorrection = correction.template head<TangentDim>();
		DerivsType derivsCorrection = correction.template tail<DerivsDim>();
		
		// TODO
		FullVecType x;
		x.template head<TangentDim>() = FixedVectorType<TangentDim>::Zero();
		x.template tail<DerivsDim>() = _derivs;
		UpdateInfo info;
		info.xpre = x;
		info.Spre = _cov;

		_pose = _pose * PoseType::Exp( poseCorrection );
		_derivs = _derivs + derivsCorrection;
		_cov = _cov - _cov * Cfull.transpose() * Vinv.solve( Cfull * _cov );

		x.template head<TangentDim>() = FixedVectorType<TangentDim>::Zero();
		x.template tail<DerivsDim>() = _derivs;
		info.observation = v; // TODO
		info.innovation = v;
		info.post_innovation = PoseType::Log( _pose.Inverse() * obs );
		info.delta_x = x - info.xpre;
		info.Spost = _cov;
		info.H = Cfull;

		return info;
	}

private:

	PoseType _pose;
	DerivsType _derivs;
	FullCovType _cov;
	MultivariateGaussian<> _normal;
	TransMatFunc _tfunc;
	
};
	
}
