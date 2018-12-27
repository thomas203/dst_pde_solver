#include "solver.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdlib>
namespace dauphine
{

	mesh::mesh(double dt, double dx, double maturity, double spot, std::vector<double> boundaries)
		: m_dt(dt), m_dx(dx), m_maturity(maturity),m_spot(spot), m_spot_boundaries(boundaries)
	{
	}

	mesh::~mesh()
	{
	}
	double mesh::get_mesh_maturity() const {
		return m_maturity;
	}
	double mesh::get_mesh_dt() const {
		return m_dt/365;
	}
	double mesh::get_mesh_dx() const {
		return m_dx;
	}
	double mesh::get_mesh_spot() const {
		return m_spot;
	}

	std::vector<double> mesh::get_mesh_spot_boundaries() const {
		return m_spot_boundaries;
	}


	std::vector<double> mesh::spot_vector() {
		int size = floor((m_spot_boundaries[0] - m_spot_boundaries[1]) / m_dx)+1;
		std::vector<double> result(size-1);
		for (std::size_t i = 0; i <= result.size(); ++i)
		{
			result[i] =log(m_spot_boundaries[1]+i*m_dx);
		}
		return result;
	}


	initial_function::initial_function(double(*f)(std::vector<double>))
		: m_f(f)
	{
	}
	double initial_function::function_operator(std::vector<double> arguments)
    {
		return m_f(arguments);
	}
    
	initial_function::~initial_function()
    {
	}

 

	// les fonctions ici sont pour les calculs des coeffs de la matrice tridiagonale sub = diag dessous, diag = diagonale,
	// il y a peut �tre une erreur dans les coeffs donc il faudrait qu'on fasse tous les trois le calcul et on compare pour �tre sur
	double diag_coeff(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments) {
		if (arguments[0] == arguments[2]|| arguments[0] == arguments[3])
		{
			return 1.0;
		}
		else
		{
			return 1.0 / m.get_mesh_dt() + rate.function_operator(arguments)*arguments[4]+1.0/(m.get_mesh_dx()*m.get_mesh_dx())*arguments[4]*pow(vol.function_operator(arguments),2);
		}
	}
	double subdiag_coeff(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments) {
		if (arguments[0] == arguments[2] || arguments[0] == arguments[3])
		{
			return 0.;
		}
		else {
			return -1.0 / 2.0 * arguments[4] / (m.get_mesh_dx()*m.get_mesh_dx())*pow(vol.function_operator(arguments), 2) - 1.0 / (4.0 * m.get_mesh_dx())*arguments[4] * (pow(vol.function_operator(arguments), 2) - rate.function_operator(arguments));
		}
	}

	double updiag_coeff(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments) {
		if (arguments[0] == arguments[2] || arguments[0] == arguments[3])
		{
			return 0.;
		}
		else {
			return -1.0 / 2.0 * arguments[4] / (m.get_mesh_dx()*m.get_mesh_dx())*pow(vol.function_operator(arguments), 2) + 1.0 / (4.0 * m.get_mesh_dx())*arguments[4] * (pow(vol.function_operator(arguments), 2) - rate.function_operator(arguments));
		}
	}

    
	// ici je compute le vecteur � la maturit� pour avoir le prix � matu et pouvoir faire backward, donc c'est juste appliqu� le payoff pour le spot
	std::vector<double> initial_price_vector(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments, initial_function payoff) {
		std::vector<double> result=m.spot_vector();
		for (std::size_t i = 0; i <= result.size(); i++) {
			arguments[0] = (exp(result[i]));
			if (payoff.function_operator(arguments) == 0) {
				result[i] = 0;
			}
			else {
				result[i] = payoff.function_operator(arguments);
			}
		}
		return result;
	}

	// une fois que j'ai un vecteur de prix je voudrais le transformer en le mutlipliant par la matrice M(1-theta)
	// comme �a la partie de droite du probl�me ne sera qu'un vecteur
	std::vector<double> column_up(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments, initial_function payoff,std::vector<double> up_price) {
		std::vector<double> result = up_price;
		std::vector<double> result2 = m.spot_vector();
		arguments[4] = arguments[4] - 1;
		for (std::size_t i = 1; i < result.size()-1; i++) {
			arguments[0] = result2[i];
			result[i] = result[i] * diag_coeff(m, rate,vol ,arguments) + result[i - 1] * subdiag_coeff(m, rate, vol, arguments) + result[i + 1] * updiag_coeff(m, rate, vol, arguments);
		}
		return result;
	}

	//algo used : https://en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm
	// l� j'utilise en gros l'algo du lien au dessus pour solver, mais je pense qu'il faudrait le refaire � la main pour �tre
	// sur que �a marche vraiment, dans le wiki y a la d�mo et �a montre comment le faire � la main
	std::vector<double> price_vector(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments, initial_function payoff,std::vector<double> col_up) {
		int size = col_up.size();
		std::vector<double> result(size);
		double W = 0;
		std::vector<double> arguments_up = arguments;
		std::vector<double> arguments_down = arguments;
		std::vector<double> B(size);
		std::vector<double> D(size);
		result[0] = col_up[0];
		result[size-1] = col_up[size-1];
		for (std::size_t i = 1; i < size - 1; i++) {
			arguments_down[0] = arguments[3] + (i - 1)*m.get_mesh_dx();
			arguments[0] = arguments[3] + i*m.get_mesh_dx();
			arguments_up[0] = arguments[3] + (i + 1)*m.get_mesh_dx();
			W = subdiag_coeff(m, rate, vol, arguments) / diag_coeff(m, rate, vol, arguments_down);
			B[i] = diag_coeff(m, rate, vol, arguments) - W*updiag_coeff(m, rate, vol, arguments_down);
			D[i] = col_up[i] - W*col_up[i - 1];
			}
		for (std::size_t i = col_up.size() - 2; i >0; i--) {
			arguments[0] = arguments[3] + i*m.get_mesh_dx();
			result[i] = (D[i] -updiag_coeff(m,rate,vol,arguments)*result[i+1])/B[i];
			i = i;
		}

		return result;
	}


/*	//une fois que j'ai l'algo pour solver au dessus je boucle jusqu'� arriver � t=0 et avoir le prix initial
	std::vector<double> price_today(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments, initial_function payoff) {
		std::vector<double> ini_price(initial_price_vector(m, rate, vol, arguments, payoff));
		std::vector<double> col_up(column_up(m, rate, vol, arguments, payoff, ini_price));
		double dt = m.get_mesh_dt();
		std::vector<double> result2 = col_up;
		int nb_step =floor( arguments[1] / dt);
		std::vector<double> result1(col_up.size());
		for (int i = 1; i < nb_step; i++) {
			arguments[1] = arguments[1] - m.get_mesh_dt();
			result1=(price_vector(m, rate, vol, arguments, payoff, result2));
			result2 = column_up(m,rate,vol,arguments,payoff,result1);
		}
		return result1;
	} */
    
    // AUTRE METHODE - TEST
    
   /* std::vector<double> spot_vector_bis(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments)
    {
        double down=log(arguments[0])-5.0*vol.function_operator(arguments)*sqrt(arguments[0]);
        double up=log(arguments[0])+5.0*vol.function_operator(arguments)*sqrt(arguments[0]);
        
        int size = floor((up - down) / m_dx)+1;
        std::vector<double> result(size-1);
        for (std::size_t i = 0; i <= result.size(); ++i)
        {
            result[i] =log(m_spot_boundaries[1]+i*m_dx);
        }
        return result;
        
    } */
    
    
    std::vector<double> diag_vector(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments)
    {
        std::vector<double> a=m.spot_vector();
        int size = a.size();
        std::vector<double> result(size,0.0);
        result[0] =1.0;
        result[size] =1.0;
        for (std::size_t i = 1; i < size; ++i)
        {
          //  result[i] =((1.0 / m.get_mesh_dt()) + (rate.function_operator(arguments)*arguments[4])+(1.0/(m.get_mesh_dx()*m.get_mesh_dx()))*arguments[4]*pow(vol.function_operator(arguments),2));
            result[i]=1.0+arguments[4]*m.get_mesh_dt()*((pow(vol.function_operator(arguments),2)/pow(m.get_mesh_dx(),2))+rate.function_operator(arguments));
        
        
        }
        return result;
    }
    
    
    std::vector<double> sub_vector(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments)
    {
        std::vector<double> a=m.spot_vector();
        int size = a.size()-1;
        std::vector<double> result(size,0.0);
        //result[0] =1.0;
        result[size] =0.0;
        for (std::size_t i = 0; i < size; ++i)
        {
            //result[i] =((-1.0 / 2.0) * (arguments[4] / (m.get_mesh_dx()*m.get_mesh_dx()))*pow(vol.function_operator(arguments), 2) - (1.0 / (4.0 * m.get_mesh_dx()))*arguments[4] * (pow(vol.function_operator(arguments), 2) - rate.function_operator(arguments)));
            result[i]=-0.5*arguments[4]*m.get_mesh_dt()*((pow(vol.function_operator(arguments),2)/pow(m.get_mesh_dx(),2))+((pow(vol.function_operator(arguments),2)-rate.function_operator(arguments))/(2.0*m.get_mesh_dx())));
            
        }
        return result;
    }
    
    
    std::vector<double> up_vector(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments)
    {
        std::vector<double> a=m.spot_vector();
        int size = a.size()-1;
        std::vector<double> result(size,0.0);
        result[0] =0.0;
        //result[1] =0.0;
        for (std::size_t i = 1; i < size+1; ++i)
        {
           // result[i] =((-1.0 / 2.0) * (arguments[4] / (m.get_mesh_dx()*m.get_mesh_dx()))*pow(vol.function_operator(arguments), 2) + (1.0 / (4.0 * m.get_mesh_dx()))*arguments[4] * (pow(vol.function_operator(arguments), 2) - rate.function_operator(arguments)));
            
        result[i]=0.5*arguments[4]*m.get_mesh_dt()*((-pow(vol.function_operator(arguments),2)/pow(m.get_mesh_dx(),2))+((pow(vol.function_operator(arguments),2)-rate.function_operator(arguments))/(2.0*m.get_mesh_dx())));
        }
        return result;
    }

    
  /*
    void tridiag_algorithm(const std::vector<double>& a,
                          const std::vector<double>& b,
                          const std::vector<double>& c,
                          const std::vector<double>& d,
                          std::vector<double>& f)
    {
        size_t N = d.size();
        
        // Create the temporary vectors
        // Note that this is inefficient as it is possible to call
        // this function many times. A better implementation would
        // pass these temporary matrices by non-const reference to
        // save excess allocation and deallocation
        std::vector<double> c_star(N, 0.0);
        std::vector<double> d_star(N, 0.0);
        
        // This updates the coefficients in the first row
        // Note that we should be checking for division by zero here
        c_star[0] = c[0] / b[0];
        d_star[0] = d[0] / b[0];
        
        // Create the c_star and d_star coefficients in the forward sweep
        for (int i=1; i<N; i++) {
            double m = 1.0 / (b[i] - a[i] * c_star[i-1]);
            c_star[i] = c[i] * m;
            d_star[i] = (d[i] - a[i] * d_star[i-1]) * m;
        }
        
        // This is the reverse sweep, used to update the solution vector f
        for (long i=N-1; i-- > 0; )
        {
            f[i] = d_star[i] - c_star[i] * d[i+1];
        }
    } */
    
    
    // Triadig algo qui fonctionne !
    std::vector<double> tridiagonal_solver(std::vector<double> & a, std::vector<double> & b, std::vector<double> & c, std::vector<double> & f)
    {
        
        int n = f.size();
        std::vector<double> x(n);
        
        for(int i=1; i<n; i++){
            
            double m = a[i-1]/b[i-1];
            b[i] -= m*c[i-1];
            f[i] -= m*f[i-1];
        }
        // solve for last x value
        x[n-1] = f[n-1]/b[n-1];
        
        // solve for remaining x values by back substitution
        for(int i=n-2; i >= 0; i--)
            x[i] = (f[i]- c[i]*x[i+1])/b[i];
        
        return x;
        
    }
    
    
   std::vector<double> price_today(mesh m, initial_function rate, initial_function vol, std::vector<double> arguments, initial_function payoff)
    {
        
       std::vector<double> f=initial_price_vector(m, rate, vol, arguments, payoff);
        
        long N=f.size();
        std::vector<double> d(N, 0.0);
        
        double dt = m.get_mesh_dt();

        int nb_step =floor( arguments[1] / dt);
    
        std::vector<double> arg=arguments;
        arg[4] = arguments[4] - 1; //theta-1
        
        //Coeffs de la Matrice
        std::vector<double> a=sub_vector(m,rate,vol,arguments);
        std::vector<double> b=diag_vector(m,rate,vol,arguments);
        std::vector<double> c=up_vector(m,rate,vol,arguments);
        
        for (long i=1; i<N; i++)
        {
            d[i] = c[i+1]*f[i+1]+b[i]*f[i]+a[i-1]*f[i-1];
            
        }

        
        
        for (int i = 0; i <nb_step ; i++)
        //for (int i=nb_step; i-- > 0; )
        {
            
          /* for (long i=1; i<N; i++)
            {
                d[i] = c[i+1]*f[i+1]+b[i]*f[i]+a[i-1]*f[i-1];
                
            } */
        
            //Condition aux bords
            
            d[N]=arguments[0];
            d[0]=0;
            
            // Now we solve the tridiagonal system
            f=tridiagonal_solver(a,b,c,d);
            d=f;
            
        }
        return f;
    }
  

/*      -----TEST - A sous forme de matrice    

// Cr�ation d'une matrice(n,m)

	double **ZeroMatrix(int n, int m)
		{
   		 double **result = (double **) malloc(n * sizeof(double*));
    		for (int row = 0; row < n; row++)
       		 result[row] = (double *) calloc(m , sizeof(double));
    		return result;
		}


// Cr�ation de la matrice A (avec les coeffs et conditions aux bords)          taille de A ????

 	double **A(mesh m, initial_function rate,initial_function vol, std::vector<double> arguments)
	{
	std::vector<double> a=m.spot_vector();
        int size = a.size();

	  double **result = ZeroMatrix(size+1, size-1);
    		
		for (int i = 0; i < size+1; i++) {
		for (int j=0; j<size-1 ;j++) { 

       		 if (i == 0 || i == dim - 1) 
		 {
           	 result[i][j] = 1;            // conditions aux bords    - quelle valeur ??
       		 } 
		
		else if(i == j) 
		{
           	 result[i][j] = 1.0+arguments[4]*m.get_mesh_dt()*((pow(vol.function_operator(arguments),2)/pow(m.get_mesh_dx(),2))+rate.function_operator(arguments));        // beta sur la diagonale 
       		} 


		else if(i == (j-1)) 
		{
           	 result[i][j] = ((-1.0 / 2.0) * (arguments[4] / (m.get_mesh_dx()*m.get_mesh_dx()))*pow(vol.function_operator(arguments), 2) + (1.0 / (4.0 * m.get_mesh_dx()))*arguments[4] * (pow(vol.function_operator(arguments), 2) - rate.function_operator(arguments)));    //updiagonale 
       		} 


		else if(i == (j+1)) 
		{
           	 result[i][j] = -0.5*arguments[4]*m.get_mesh_dt()*((pow(vol.function_operator(arguments),2)/pow(m.get_mesh_dx(),2))+((pow(vol.function_operator(arguments),2)-rate.function_operator(arguments))/(2.0*m.get_mesh_dx())));     //subdiagonale 
       		} 


	}
    }
    return result;
}




// Inversion de la matrice           <<>> cin pour param�tres entr�s par l utilisateur (cf theta) ?


*/

}

