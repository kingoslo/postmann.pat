#include <iostream>
#include <string>
#include <armadillo>
#include <random>
#include <mpi.h>
#include "basis.h"
#include "maths.h"

using namespace std;
using namespace arma;



// definere globale variable fordi jeg ønsker det.
basis B = basis(5);
int n;

random_device rd;
mt19937 gen(rd());

double laplacePsi(int i, mat r, mat invDplus, mat invDminus, double a, double b, mat c, double w);
double laplaceD(mat invDplus, mat invDminus, int i, int spin, mat r, double a, double w);
double laplaceJastrow(int k, mat r, double b, mat c);
vec delPsi(int i, mat r, mat invDplus, mat invDminus, double a, double b, mat c, double w);
vec delD(mat invDplus, mat invDminus, int i, int spin, mat r, double a, double w);
vec delJastrow(int k, mat r, double b, mat c);
mat D(mat r, int spin, double a, double w);
//double psiC(mat r, double b, mat c);
double logpsiC(mat r, double b, mat c);
double * vmc(double a,double b,mat c,double w,int n, int iters);
double dpsida(mat r, mat D, mat invD,double a,double w, int spin);
double dpsidb(mat r, double b, mat c);


int main(int nargs, char *args[])
{
    n = atof(args[3]);
    int iterations = pow(2,15);
    mat c = ones<mat>(n,n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j< n; j++) {
             if (i % 2 == j % 2)
                 c(i,j) = 1/3.;
        }
    }
    double a = atof(args[1]);
    double b = atof(args[2]);
    double w = atof(args[4]);
    double E;
    double apsi;
    double bpsi;
    double eapsi;
    double ebpsi;
    vec xpp {a,b};
    vec x = xpp + ones<vec>(2)*0.1;
    vec delF = zeros<vec>(2);
    int i = 0;
    MPI_Init(NULL, NULL);
    int numprocs, my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    while (true) {
        double * data;
        data = vmc(xpp[0],xpp[1],c,w,n,iterations);
        MPI_Allreduce(&data[2], &E, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&data[7], &apsi, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&data[8], &bpsi, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&data[9], &eapsi, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&data[10], &ebpsi, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        delF[0] = 2*( eapsi/numprocs - E/numprocs*apsi/numprocs );
        delF[1] = 2*( ebpsi/numprocs - E/numprocs*bpsi/numprocs );
        double g = 0.0001;
        x = xpp - g*delF;
        //if (norm(delF) < 1e-4) {
        //    x.print();
        //    break;
        //}
        //if (i % 10 == 0) {
            if (my_rank == 0) {
                x.print();
                cout << endl;
                delF.print();
                cout << E/numprocs << endl;
                cout << "*******************************" << endl;
            }
        //}
        xpp = x;
        i++;
    }

    MPI_Finalize();
    //cout << data[0] << " " << data[1] << " " << data[2] << " " << data[3] << " " << data[4] << " " << data[5] << " " << data[6] << endl;
    return 0;
}

double * vmc(double a,double b,mat c,double w,int n, int iters) {
    uniform_int_distribution<int> rand_particle(0, n-1);
    uniform_int_distribution<int> rand_bool(0, 1);
    uniform_real_distribution<double> rand_double(0.0,1.0);
    mat r = randn<mat>(2,n)*sqrt(1./w);
    mat Dplus = D(r, 0, a, w);
    mat Dminus = D(r, 1, a, w);
    mat invDplus  = inv(Dplus);
    mat invDminus = inv(Dminus);

    mat rpp = r;
    mat Dpluspp = Dplus;
    mat Dminuspp = Dminus;
    mat invDpluspp  = invDplus;
    mat invDminuspp = invDminus;
    vec F = zeros<vec>(2);
    vec Fpp = zeros<vec>(2);
    vec p,q;

    F = 2*delPsi(0, r, invDplus, invDminus, a, b, c, w);
    Fpp = 2*delPsi(1, r, invDplus, invDminus, a, b, c, w);

    double dt = 0.005; double d = 0.5;
    double e; double E = 0; double V = 0; double K = 0; double R = 0; double ki; double ri; double vi;
    int u = 0; int v = 0; int s;
    double logdet, logdetpp, logJastrow, logJastrowpp, logG;
    double apsi = 0;
    double bpsi = 0;
    double eapsi = 0;
    double ebpsi = 0;


    double time_start = MPI_Wtime();
    for (u = 0; u < iters; u++) {
        s = rand_particle(gen);
        Fpp = 2*delPsi(s,rpp,invDpluspp,invDminuspp,a,b,c,w);
        r = rpp;
        r.col(s) = rpp.col(s) + d*Fpp*dt + randn<vec>(2)*sqrt(dt);

        Dplus  = D(r, 0, a, w);
        Dminus = D(r, 1, a, w);
        if (abs(det(Dminus)) < 1e-1 or abs(det(Dminus)) < 1e-1)
            continue;
        invDplus = inv(Dplus);
        invDminus = inv(Dminus);

        //cout << invDplus(0,0)*B.psi(0,0,sqrt(a)*r(0,0),sqrt(a)*r(1,0),w) << endl;
        //exit(1);
        F = 2*delPsi(s,r,invDplus,invDminus,a,b,c,w);
        p = rpp.col(s) - r.col(s) - d*dt*F;
        q = r.col(s) - rpp.col(s) - d*dt*Fpp;
        logG = abs((dot(q,q) - dot(p,p))/(4*d*dt));
        logdet = log( abs(det(Dplus) *det(Dminus) ) );
        logJastrow = logpsiC(r,b,c);
        if ( logG + 2*(logdet - logdetpp + logJastrow - logJastrowpp) > log(rand_double(gen)) ) {
            rpp = r; Dpluspp = Dplus; Dminuspp = Dminus; invDpluspp = invDplus;
            invDminuspp = invDminus; logdetpp = logdet; logJastrowpp = logJastrow;
            v++;
        }


        // sample energy
        e = 0; ki = 0; vi = 0;
        for (int i = 0; i < n; i++) {
            //cout << laplacePsi(i, rpp, invDpluspp, invDminuspp, a, b, c, w) << endl;
            //exit(1);
            ki += -0.5*laplacePsi(i, rpp, invDpluspp, invDminuspp, a, b, c, w);
            vi += 0.5*w*w*dot(rpp.col(i),rpp.col(i));
            e += -0.5*laplacePsi(i, rpp, invDpluspp, invDminuspp, a, b, c, w) + 0.5*w*w*dot(rpp.col(i),rpp.col(i));
            for (int j = i+1; j < n; j++) {
                vi += 1./norm(rpp.col(i) - rpp.col(j));
                e += 1./norm(rpp.col(i) - rpp.col(j));
            }
        }
        double rij = norm(r.col(0) - r.col(1));
        //double tmpaa = -0.5*w*(dot(r.col(0),r.col(0)) + dot(r.col(1),r.col(1)) );
        //double tmpbb = -rij*rij/pow(1 + b*rij,2);
        double tmpa = dpsida(r, Dplus, invDplus, a, w, 0) + dpsida(r, Dminus, invDminus, a, w, 1);
        double tmpb = dpsidb(r, b, c);
        /*
        if ( abs(tmpaa - tmpa ) >1e-7) {
            cout << u << ": " << tmpaa << " " << tmpa << endl;
            exit(1);
        }*/
        apsi += tmpa/iters;
        bpsi += tmpb/iters;
        eapsi += e*tmpa/iters;
        ebpsi += e*tmpb/iters;
        E += e/iters; V += vi/iters; K += ki/iters;
    }
    double *data = new double [11];
    data[0] = a;
    data[1] = b;
    data[2] = E;
    data[3] = K;
    data[4] = V;
    data[5] = v/iters;
    data[6] = MPI_Wtime() - time_start;
    data[7] = apsi;
    data[8] = bpsi;
    data[9] = eapsi;
    data[10] = ebpsi;
    return data;
}

double dpsida(mat r, mat D, mat invD,double a,double w, int spin) {
    double sum = 0;
    for (int i = 0; i < n/2; i++) {
        for (int j = 0; j < n/2; j++) {
            double x = r(0,2*j + spin);
            double y = r(1,2*j + spin);
            int nx = B.get_state(2*i + 1 + spin)[0];
            int ny = B.get_state(2*i + 1 + spin)[1];
            sum += invD(i,j)*( x*nx*sqrt(w/a)*B.psi(nx-1,ny,sqrt(a)*x,sqrt(a)*y,w) + y*ny*sqrt(w/a)*B.psi(nx,ny-1,sqrt(a)*x,sqrt(a)*y,w)
                               - 0.5*w*dot(r.col(2*j+spin),r.col(2*j+spin))*B.psi(nx, ny, sqrt(a)*x, sqrt(a)*y,w) );
        }
    }
    //cout << sum + 0.5*w*dot(r.col(spin),r.col(spin)) << endl;
    return sum;
}

double dpsidb(mat r, double b, mat c) {
    double sum = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            double rij = norm(r.col(i) - r.col(j));
            sum += c(i,j)*pow( rij/(1+b*rij),2);
        }
    }
    return -sum;
}

double laplaceJastrow(int k, mat r, double b, mat c) {
    double x = 0; double y = 0; double xy = 0; double rkj;
    for (int j = 0; j < n; j++) {
        if (j == k)
            continue;
        rkj = norm(r.col(k) - r.col(j));
        x += c(k,j)*(r(0,k) - r(0,j))/( pow(1 + b*rkj,2)*rkj);
        y += c(k,j)*(r(1,k) - r(1,j))/( pow(1 + b*rkj,2)*rkj);
        xy += c(k,j)*(1 - b*rkj)/(pow(1 + b*rkj,3)*rkj);
    }
    return x*x + y*y + xy;
}

vec delJastrow(int k, mat r, double b, mat c) {
    vec sum = zeros<vec>(2);
    for (int j = 0; j <n; j++) {
        if (k == j)
            continue;
        double rjk = norm(r.col(j) - r.col(k));
        double tmp = c(j,k)/(pow(1 + b*rjk,2)*rjk);
        sum[0] += tmp*(r(0,k) - r(0,j));
        sum[1] += tmp*(r(1,k) - r(1,j));
    }
    return sum;
}

vec delPsi(int i, mat r, mat invDplus, mat invDminus, double a, double b, mat c, double w) {
    return delD(invDplus,invDminus, i, i % 2, r, a, w) + delJastrow(i, r, b, c);
}


double laplaceD(mat invDplus, mat invDminus, int i, int spin, mat r, double a,double w) {
    double sum = 0;
    mat invD;
    if (spin == 1) {
        invD = invDminus;
    } else {
        invD = invDplus;
    }
    for (int j = 0; j < n/2; j ++ ) { // sjekk indekser her
        int nx = B.get_state(2*j + 1 + spin)[0];
        int ny = B.get_state(2*j + 1 + spin)[1];
        double x = r(0,i);
        double y = r(1,i);

        sum += invD(j,i/2)*(4*a*w*(nx*(nx-1)*B.psi(nx-2,ny,sqrt(a)*x,sqrt(a)*y,w) + ny*(ny-1)*B.psi(nx,ny-2,sqrt(a)*x,sqrt(a)*y,w))
               - 4*pow(a*w,1.5)*(x*nx*B.psi(nx-1,ny,sqrt(a)*x,sqrt(a)*y,w) + y*ny*B.psi(nx,ny-1,sqrt(a)*x,sqrt(a)*y,w) )
               + a*w*B.psi(nx,ny,sqrt(a)*x,sqrt(a)*y,w)*(a*w*dot(r.col(i),r.col(i)) - 2));
    }
    return sum;
}

vec delD(mat invDplus, mat invDminus, int i, int spin, mat r, double a, double w) {
    vec sum = zeros<vec>(2);
    mat invD;
    if (spin == 1) {
        invD = invDminus;
    } else {
        invD = invDplus;
    }

    for (int j = 0; j <n/2; j++) {
        int nx = B.get_state(2*j + 1 + spin)[0];
        int ny = B.get_state(2*j + 1 + spin)[1];
        double x = r(0,i);
        double y = r(1,i);
        sum[0] += invD(j,i/2)*(2*nx*sqrt(a*w)*B.psi(nx-1,ny,sqrt(a)*x,sqrt(a)*y,w) - a*w*x*B.psi(nx,ny,sqrt(a)*x,sqrt(a)*y,w));
        sum[1] += invD(j,i/2)*(2*ny*sqrt(a*w)*B.psi(nx,ny-1,sqrt(a)*x,sqrt(a)*y,w) - a*w*y*B.psi(nx,ny,sqrt(a)*x,sqrt(a)*y,w));
    }
    return sum;
}

mat D(mat r, int spin, double a, double w) {
    mat D = ones<mat>(n/2,n/2);
    for (int i = 0; i < n/2; i++) {
        for (int j = 0; j< n/2; j++) {
            int nx = B.get_state(2*j+1+spin)[0];
            int ny = B.get_state(2*j+1+spin)[1];
            D(i,j) = B.psi(nx,ny,sqrt(a)*r(0,2*i+spin),sqrt(a)*r(1,2*i+spin),w);
        }
    }
    return D;
}
/*
double psiC(mat r, double b, mat c) {
    double jastrow = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            double rij = norm(r.col(i) - r.col(j));
            jastrow += c(i,j)*rij/(1+b*rij);
        }
    }
    return exp(jastrow);
}
*/

double logpsiC(mat r, double b, mat c) {
    double jastrow = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            double rij = norm(r.col(i) - r.col(j));
            jastrow += c(i,j)*rij/(1+b*rij);
        }
    }
    return jastrow;
}


double laplacePsi(int i, mat r, mat invDplus, mat invDminus, double a, double b, mat c, double w) {
    double sum = 0;
    int spin = i % 2; // 0 impliserer spin opp
    // compute laplace D+/- avhengig av spin
    sum += laplaceD(invDplus, invDminus, i,spin, r,a,w);
    // compute laplace jastrow
    sum += laplaceJastrow(i, r, b, c);
    // compute del D+/- dot del jastrow
    sum += 2*dot(delD(invDplus, invDminus, i, spin, r, a, w), delJastrow(i, r, b, c));
    // return sum of terms
    return sum;
}


/*
    // for 2 partikkel, w = 1 regne ut produktet delJastrow*delpsi0
    cout << Dplus(0,0)*Dminus(0,0)*psiC(r, b, c) << endl;
    cout << psi(r.col(0), r.col(1), a, b, 1, w) << endl;

    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += dot( delD(invDplus,invDminus, i, i % 2, r, a, w), delJastrow(i, r, b, c) );
    }
    double exact = -a*1*w*norm(r.col(0) - r.col(1))/pow(1 + b*norm(r.col(0) - r.col(1)),2);
//    cout << "Exact answer: " << exact << endl;
    cout << "Numerical answer: " << sum << endl;

//    cout << "LaplaceD_1 numerics: " << laplaceD(invDplus, invDminus, 1,1, r,a,w) << endl;
//    cout << laplaceD(invDplus, invDminus, 0,0, r,a,w) << endl;
//    cout << "LaplaceD_1 exact: " << a*a*w*w*dot(r.col(1),r.col(1)) - 2*a*w << " " << a*a*w*w*dot(r.col(0),r.col(0)) - 2*a*w << endl;

    cout << "Laplace jastrow numerics: " << laplaceJastrow(0,r,a,b,c) << endl;
    double r12 = norm(r.col(0) - r.col(1) );
    cout << "Laplace jastrow exact: " << (1 + r12 - b*b*r12*r12 )/(r12*pow(1 + b*r12,4))  << endl;
    */
