#ifndef LINALG_H 
#define LINALG_H 

#include <vector>
#include <cmath>

namespace peopt {
    template <typename T>
    void copy(int n,const T* x,int incx,T* y,int incy);
    
    template <typename T>
    void axpy(int n,T alpha,const T* x,int incx,T* y,int incy);
    
    template <typename T>
    void scal(int n,const T alpha,T* x,int incx);
    
    template <typename T>
    T dot(int n,const T* x,int incx,const T* y,int incy);
    
    template <typename T>
    void syr2k(char uplo,char trans,int n,int k,T alpha,const T* A,int lda,
        const T* B,int ldb,T beta,T* C,int ldc);

    template <typename T>
    void syevr(char jobz,char range,char uplo,int n,T *A,int lda,
        T vl,T vu,int il,int iu,T abstol,int& m,
        T* w,T* z,int ldz,int* isuppz,T* work,int lwork,
        int* iwork,int liwork,int& info);
    
    template <typename T>
    void stemr(char jobz,char range,int n,T *D,T *E,T vl,T vu,int il,int iu,
        int& m,T* w,T* z,int ldz,int nzc,int* isuppz,int& tryrac,T* work,
        int lwork,int* iwork,int liwork,int& info);
    
    template <typename T>
    void stevr(char jobz,char range,int n,T *D,T *E,T vl,T vu,int il,int iu,
        T abstol, int& m,T* w,T* z,int ldz,int* isuppz,T* work,
        int lwork,int* iwork,int liwork,int& info);

    template <typename T>
    T lamch(char cmach);

    template <typename T>
    void gemm(char transa,char transb,int m,int n,int k,T alpha,
        const T* A,int lda,const T* B,int ldb,T beta,
        T* C,int ldc);
    
    template <typename T>
    void symm(char side,char uplo,int m,int n,T alpha,const T* A,
        int lda,const T* B,int ldb,T beta,T* C,int ldc);
    
    template <typename T>
    void symv(char uplo,int n,T alpha,const T* A,int lda,const T* x,int incx,
        T beta,T* y,int incy);

    template <typename T>
    void potrf(char uplo,int n,T* A,int lda,int& info);

    template <typename T>
    void trtri(char uplo,char diag,int n,T* A,int lda,int& info);

    // Absolute value
    template <typename T>
    T abs(T alpha);

    // Square root
    template <typename T>
    T sqrt(T alpha);

    // Logarithm
    template <typename T>
    T log(T alpha);
    
    // Indexing function for matrices
    unsigned int ijtok(unsigned int i,unsigned int j,unsigned int m);

    /* Given a Schur decomposition of A, A=V D V', solve the Sylvester equation
    
       A X + X A = B

    */
    template <typename T>
    void sylvester(
        const unsigned int m,
        const T* V,
        const T* D,
        const T* B,
        T* X
    ) {

        // Find V' B V
        std::vector <T> tmp(m*m);
        std::vector <T> VtBV(m*m);
        // tmp <- B V
        symm <T> ('L','U',m,m,T(1.),&(B[0]),m,&(V[0]),m,T(0.),&(tmp[0]),m); 
        // VtBV <- V' B V
        gemm <T> ('T','N',m,m,m,T(1.),&(V[0]),m,&(tmp[0]),m,T(0.),&(VtBV[0]),m);

        // Solve for each column of X.  In theory, we only need half of these
        // elements since X is symmetric.
        #pragma omp parallel for schedule(static)
        for(unsigned int j=1;j<=m;j++) {
            for(unsigned int i=1;i<=j;i++) 
                X[ijtok(i,j,m)]=VtBV[ijtok(i,j,m)]/(D[i-1]+D[j-1]);
        }

        // Transform the solution back, X = V X V'
        // tmp <- V X
        symm <T> ('R','U',m,m,T(1.),&(X[0]),m,&(V[0]),m,T(0.),&(tmp[0]),m);
        // X <- V X V'
        gemm <T> ('N','T',m,m,m,T(1.),&(tmp[0]),m,&(V[0]),m,T(0.),&(X[0]),m);
    }

    // Find a bound on the smallest eigenvalue of the given matrix A such
    // that lambda_min(A) < alpha where alpha is returned from this function.
    template <typename T>
    T lanczos(
        const unsigned int m,
        const T* A,
        const unsigned int max_iter,
        const T tol
    ) {
        // Create the initial Krylov vector
        std::vector <T> v(m,T(1./std::sqrt(m)));

        // Get the next Krylov vector and orthgonalize it
        std::vector <T> w(m);
        // w <- A v
        symv <T> ('U',m,T(1.),&(A[0]),m,&(v[0]),1,T(0.),&(w[0]),1);
        // alpha[0] <- <Av,v>
        std::vector <T> alpha;
        alpha.push_back(dot <T> (m,&(w[0]),1,&(v[0]),1));
        // w <- Av - <Av,v> v
        axpy <T> (m,-alpha[0],&(v[0]),1,&(w[0]),1);

        // Store the norm of the Arnoldi vector w in the off diagonal part of T.
        // By T, we mean the triagonal matrix such that A = Q T Q'.
        std::vector <T> beta;
        beta.push_back(std::sqrt(dot <T> (m,&(w[0]),1,&(w[0]),1)));

        // Allocate memory for solving an eigenvalue problem for the Ritz
        // values and vectors later.
        std::vector <int> isuppz;
        std::vector <T> work(1);
        std::vector <int> iwork(1);
        int lwork=-1;
        int liwork=-1;
        int info;
        int nevals;
        //int nzc=0;
        std::vector <T> W;
        std::vector <T> Z;
        std::vector <T> D;
        std::vector <T> E;

        // Start Lanczos
        std::vector <T> v_old(m);
        for(unsigned int i=0;i<max_iter;i++) {
            // Save the current Arnoldi vector
            copy <T> (m,&(v[0]),1,&(v_old[0]),1);

            // Copy the candidate Arnoldi vector to the current Arnoldi vector
            copy <T> (m,&(w[0]),1,&(v[0]),1);

            // Get the normalized version of this vector.  This is now a real
            // Arnoldi vector.
            scal <T> (m,T(1.)/beta[i],&(v[0]),1);

            // Get the new Arnoldi vector, w <- A v
            symv <T> ('U',m,T(1.),&(A[0]),m,&(v[0]),1,T(0.),&(w[0]),1);

            // Orthogonalize against v_old and v using modified Gram-Schdmit.

            // First, we orthogonalize against v_old
            // w <- Av - <Av,v_old> v_old.  Due to symmetry, <Av,v_old>=beta.
            axpy <T> (m,-beta[i],&(v_old[0]),1,&(w[0]),1);

            // Now, we orthogonalize against v
            // Find the Gram-Schmidt coefficient
            alpha.push_back(dot <T> (m,&(w[0]),1,&(v[0]),1));
            // Orthogonlize w to v
            axpy <T> (m,-alpha[i+1],&(v[0]),1,&(w[0]),1);

            // Store the norm of the Arnoldi vector w in the off diagonal part
            // of T.
            beta.push_back(std::sqrt(dot <T> (m,&(w[0]),1,&(w[0]),1)));
   
   #if 0
            // Figure out the workspaces for the eigenvalues and eigenvectors
            int k=alpha.size();  // Size of the eigenvalue subproblem
            D.resize(alpha.size());
            copy <T> (k,&(alpha[0]),1,&(D[0]),1);
            E.resize(beta.size());
            copy <T> (k,&(beta[0]),1,&(E[0]),1);
            isuppz.resize(2*k);
            lwork=-1;
            liwork=-1;
            W.resize(k);
            Z.resize(k*k);
            peopt::stemr <double> ('V','A',k,&(D[0]),&(E[0]),double(0.),
                double(0.),0,0,nevals,&(W[0]),&(Z[0]),k,k,&(isuppz[0]),
                nzc,&(work[0]),lwork,&(iwork[0]),liwork,info);

            // Resize the workspace 
            lwork = int(work[0])+1;
            work.resize(lwork);
            liwork = iwork[0];
            iwork.resize(liwork);

            // Find the eigenvalues and vectors 
            peopt::stemr <double> ('V','A',k,&(D[0]),&(E[0]),double(0.),
                double(0.),0,0,nevals,&(W[0]),&(Z[0]),k,k,&(isuppz[0]),
                nzc,&(work[0]),lwork,&(iwork[0]),liwork,info);
#else
            // Figure out the workspaces for the eigenvalues and eigenvectors
            int k=alpha.size();  // Size of the eigenvalue subproblem
            D.resize(alpha.size());
            copy <T> (k,&(alpha[0]),1,&(D[0]),1);
            E.resize(beta.size());
            copy <T> (k,&(beta[0]),1,&(E[0]),1);
            isuppz.resize(2*k);
            lwork=-1;
            liwork=-1;
            W.resize(k);
            Z.resize(k*k);
            peopt::stevr <T> ('V','A',k,&(D[0]),&(E[0]),T(0.),
                T(0.),0,0,peopt::lamch <T> ('S'),nevals,&(W[0]),&(Z[0]),k,
                &(isuppz[0]),&(work[0]),lwork,&(iwork[0]),liwork,info);

            // Resize the workspace 
            lwork = int(work[0])+1;
            work.resize(lwork);
            liwork = iwork[0];
            iwork.resize(liwork);

            // Find the eigenvalues and vectors 
            peopt::stevr <T> ('V','A',k,&(D[0]),&(E[0]),T(0.),
                T(0.),0,0,peopt::lamch <T> ('S'),nevals,&(W[0]),&(Z[0]),k,
                &(isuppz[0]),&(work[0]),lwork,&(iwork[0]),liwork,info);
#endif

            // Find beta_i |s_{ik}| where s_{ik} is the ith (last) element
            // of the kth Ritz vector where k corresponds to the largest
            // and smallest Ritz values.  Basically, we don't know which is
            // going to converge first, but they'll both be the first two.
            // Hence, we converge until these errors estimates are accurate
            // enough.
            T err_est_min = abs <T> (Z[ijtok(k,1,k)])*beta[i+1];
            T err_est_max = abs <T> (Z[ijtok(k,k,k)])*beta[i+1];

            // Stop of the error estimates are small
            if(    err_est_min < abs <T> (W[0]) * tol
                && err_est_max < abs <T> (W[i]) * tol
            )
                break;
        }

        // Return the smallest Ritz value
        return W[0];
    }
}

#endif

#if 0
namespace peopt {

    // Vector space for the nonnegative orthant.  For basic vectors
    // in R^m, use this.
    template <typename Real>
    struct Rm;

    // This contains the pieces generic for both float and doubles
    template <typename Real>
    struct Rm_ { 
        typedef std::vector <Real> Vector;

        // Memory allocation and size setting
        static void init(const Vector& x, Vector& y) {
            y.resize(x.size());
        }

        // x <- 0 
        static void zero(Vector& x) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                x[i]=Real(0.);
        }

        // Jordan product, z <- x o y
        static void prod(const Vector& x, const Vector& y, Vector& z) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                z[i]=x[i]*y[i];
        }

        // Identity element, x <- e such that x o e = x
        static void id(Vector& x) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                x[i]=1.;
        }
        
        // Jordan product inverse, z <- inv(L(x)) y where L(x) y = x o y
        static void linv(const Vector& x,const Vector& y,Vector& z) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                z[i]=(1/x[i])*y[i];
        }

        // Barrier function, barr <- barr(x) where x o grad barr(x) = e
        static Real barr(const Vector& x) {
            Real z=0;
            #pragma omp parallel for reduction(+:z) schedule(static)
            for(unsigned int i=0;i<x.size();i++)
                z+=std::log(x[i]);
            return z;
        }

        // Line search, srch <- argmax {alpha \in Real >= 0 : alpha x + y >= 0}
        // where y > 0.  If the argmax is infinity, then return Real(-1.).
        static Real srch(const Vector& x,const Vector& y) {
            // Line search parameter
            Real alpha=Real(-1.);

            #pragma omp parallel
            {
                // Create a local version of alpha
                Real alpha_loc=Real(-1.);

                // Search for the optimal linesearch parameter
                #pragma omp parallel for schedule(static)
                for(unsigned int i=0;i<x.size();i++) {
                    if(x[i] < 0) {
                        Real alpha0 = -y[i]/x[i];
                        if(alpha_loc==Real(-1.) || alpha0 < alpha_loc)
                            alpha_loc=alpha0;
                    }
                }

                // After we're through with the local search, accumulate the
                // result
                #pragma omp critical
                {
                   if(alpha==Real(-1.) || alpha_loc < alpha)
                       alpha=alpha_loc;
                }
            }
            return alpha;
        }
    };

    // This contains the BLAS calls for doubles
    template <>
    struct Rm <double> : public Rm_ <double> { 
        typedef double Real;
        typedef std::vector <Real> Vector;

        // y <- x (Shallow.  No memory allocation.)
        static void copy(const Vector& x, Vector& y) {
        }

        // x <- alpha * x
        static void scal(const Real& alpha, Vector& x) {
        }

        // y <- alpha * x + y
        static void axpy(const Real& alpha, const Vector& x, Vector& y) {
        }

        // innr <- <x,y>
        static Real innr(const Vector& x,const Vector& y) {
        }
    };
    
    // This contains the BLAS calls for floats 
    template <>
    struct Rm <float> : public Rm_ <float> { 
        typedef float Real;
        typedef std::vector <Real> Vector;

        // y <- x (Shallow.  No memory allocation.)
        static void copy(const Vector& x, Vector& y) {
            cblas_scopy(x.size(),&(x[0]),1,&(y[0]),1);
        }

        // x <- alpha * x
        static void scal(const Real& alpha, Vector& x) {
            cblas_sscal(x.size(),alpha,&(x[0]),1);
        }

        // y <- alpha * x + y
        static void axpy(const Real& alpha, const Vector& x, Vector& y) {
            cblas_saxpy(x.size(),alpha,&(x[0]),1,&(y[0]),1);
        }

        // innr <- <x,y>
        static Real innr(const Vector& x,const Vector& y) {
            return cblas_sdot(x.size(),&(x[0]),1,&(y[0]),1);
        }
    };

    // Vector space for the cone of positive semidefinite matrices
    template <typename Real>
    struct PositiveSemidefiniteMatrices {
        typedef std::vector <Real> Vector;
        
        // Memory allocation and size setting
        static void init(const Vector& x, Vector& y) {
            y.resize(x.size());
        }

        // x <- 0 
        static void zero(Vector& x) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                x[i]=Real(0.);
        }

        // Jordan product, z <- x o y
        static void prod(const Vector& x, const Vector& y, Vector& z) {
            // Get the size
            unsigned int m=std::sqrt(x.size());

            // Fill the lower half of x and y
            #pragma omp parallel for schedule(guided)
            for(int j=1;j<=m;j++)
                for(int i=j+1;i<=m;i++) {
                    x[ijtok(i,j,m)]=x[ijtok(j,i,m)];
                    y[ijtok(i,j,m)]=y[ijtok(j,i,m)];
                }

            // Compute the symmetric product
            cblas_dsyr2k(CblasColMajor,CblasUpper,m,Real(0.5),&(x[0]),1,
                &(y[0]),1,&(z[0]),m);
        }

        // Identity element, x <- e such that x o e = x
        static void id(Vector& x) {
            // Zero out the matrix
            zero(x);
            
            // Set the diagonal elements to 1
            unsigned int m=std::sqrt(x.size());
            #pragma omp parallel for schedule(static)
            for(unsigned int i=1;i<=m;i++) 
                x[ijtok(i,i,m)]=1.;
        }
        
        // Jordan product inverse, z <- inv(L(x)) y where L(x) y = x o y
        static void linv(const Vector& x,const Vector& y,Vector& z) {
            #pragma omp parallel for schedule(static)
            for(unsigned int i=0;i<x.size();i++) 
                z[i]=(1/x[i])*y[i];
        }

        // Barrier function, barr <- barr(x) where x o grad barr(x) = e
        static Real barr(const Vector& x) {
            Real z=0;
            #pragma omp parallel for reduction(+:z) schedule(static)
            for(unsigned int i=0;i<x.size();i++)
                z+=std::log(x[i]);
            return z;
        }

        // Line search, srch <- argmax {alpha \in Real >= 0 : alpha x + y >= 0}
        // where y > 0.  If the argmax is infinity, then return Real(-1.).
        static Real srch(const Vector& x,const Vector& y) {
            // Line search parameter
            Real alpha=Real(-1.);

            #pragma omp parallel
            {
                // Create a local version of alpha
                Real alpha_loc=Real(-1.);

                // Search for the optimal linesearch parameter
                #pragma omp parallel for schedule(static)
                for(unsigned int i=0;i<x.size();i++) {
                    if(x[i] < 0) {
                        Real alpha0 = -y[i]/x[i];
                        if(alpha_loc==Real(-1.) || alpha0 < alpha_loc)
                            alpha_loc=alpha0;
                    }
                }

                // After we're through with the local search, accumulate the
                // result
                #pragma omp critical
                {
                   if(alpha==Real(-1.) || alpha_loc < alpha)
                       alpha=alpha_loc;
                }
            }
            return alpha;
        }

    };

}
#endif
