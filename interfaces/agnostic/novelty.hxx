/*
Lightweight Automated Planning Toolkit
Copyright (C) 2012
Miquel Ramirez <miquel.ramirez@rmit.edu.au>
Nir Lipovetzky <nirlipo@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __NOVELTY__
#define __NOVELTY__

#include <aptk/search_prob.hxx>
#include <aptk/heuristic.hxx>
#include <aptk/ext_math.hxx>
#include <strips_state.hxx>
#include <strips_prob.hxx>
#include <vector>
#include <deque>

namespace aptk {

namespace agnostic {


template <typename Search_Model >
class Novelty : public Heuristic<State> {
public:

	Novelty( const Search_Model& prob, unsigned max_arity = 1, const unsigned max_MB = 600 ) 
		: Heuristic<State>( prob ), m_strips_model( prob.task() ), m_max_memory_size_MB(max_MB) {
		
		set_arity(max_arity);
		
	}

	virtual ~Novelty() {
	}


	void init() {
		for(std::vector<State*>::iterator it = m_nodes_tuples.begin();
		    it != m_nodes_tuples.end(); it++)		
			*it = NULL;

	}

	unsigned arity() const { return m_arity; }

	void set_arity( unsigned max_arity ){
	
		m_arity = max_arity;
		m_num_tuples = 1;
		m_num_fluents = m_strips_model.num_fluents();

		float size_novelty = ( (float) pow(m_num_fluents,m_arity) / 1024000.) * sizeof(State*);
		std::cout << "Try allocate size: "<< size_novelty<<" MB"<<std::endl;
		if(size_novelty > m_max_memory_size_MB){
			m_arity = 1;
			size_novelty =  ( (float) pow(m_num_fluents,m_arity) / 1024000.) * sizeof(State*);

			std::cout<<"EXCEDED, m_arity downgraded to 1 --> size: "<< size_novelty<<" MB"<<std::endl;
		}

		for(unsigned k = 0; k < m_arity; k++)
			m_num_tuples *= m_num_fluents;

		m_nodes_tuples.resize(m_num_tuples, NULL);

	}
	
	virtual void eval( const State& s, float& h_val ) {
	
		compute( s, h_val );		
	}

	virtual void eval( const State& s, float& h_val,  std::vector<Action_Idx>& pref_ops ) {
		eval( s, h_val );
	}

	/**
	 * Use Node when possible. Faster Computation!
	 */
	template <class Node >
	void eval( const Node& n, float& h_val ) { 
		if( n.action() != no_op )
			compute( n, h_val );
		else
			compute( n.state(), h_val);
	}


protected:

	
	/**
	 * If T == Node,  the computation is F^i-1 aprox. FASTER!!!
	 * if T == State, the computation is F^i, SLOWER!! 
	 * where i ranges over 1 to max_arity
	 */
	template< typename T >
	void compute(  const T& ns, float& novelty ) 
	{

		novelty = (float) m_arity+1;
		for(unsigned i = 1; i <= m_arity; i++){
#ifdef DEBUG
			std::cout << "search state node: "<<&(ns)<<std::endl;
#endif 	
			bool new_covers = cover_tuples( ns, i );
			
#ifdef DEBUG
			if(!new_covers)	
				std::cout << "\t \t PRUNE! search node: "<<&(ns)<<std::endl;
#endif 	
			if ( new_covers )
				if(i < novelty )
					novelty = i;
		}
	}
	
	/**
	 * Instead of checking the whole state, checks the new atoms permutations only!
	 */
	template < class Node >
	bool    cover_tuples( const Node& n, unsigned arity )
	{


		const State& s = n.state();
		
		bool new_covers = false;

		std::vector<unsigned> tuple( arity );

		const Fluent_Vec& add = m_strips_model.actions()[ n.action() ]->add_vec();

		unsigned atoms_arity = arity - 1;
		unsigned n_combinations = pow( s.fluent_vec().size() , atoms_arity );
		
		
		for ( Fluent_Vec::const_iterator it_add = add.begin();
					it_add != add.end(); it_add++ )
			{
		
				tuple[ atoms_arity ] = *it_add;

				for( unsigned idx = 0; idx < n_combinations; idx++ ){

					/**
					 * get tuples from indexes
					 */
					if(atoms_arity > 0)
						idx2tuple( tuple, s, idx, atoms_arity );

					/**
					 * Check if tuple is covered
					 */
					unsigned tuple_idx = tuple2idx( tuple, arity );

					/**
					 * new_tuple if
					 * -> none was registered
					 * OR
					 * -> n better than old_n
					 */
					bool cover_new_tuple = ( !m_nodes_tuples[ tuple_idx ] ) ? true : ( is_better( m_nodes_tuples[tuple_idx], s  ) ? true : false);
                       
					if( cover_new_tuple ){
						
						m_nodes_tuples[ tuple_idx ] = (State*) &(n.state());
						new_covers = true;


#ifdef DEBUG
						std::cout<<"\t NEW!! : ";
						for(unsigned i = 0; i < arity; i++){
							std::cout<< m_strips_model.fluents()[ tuple[i] ]->signature()<<"  ";
						}
						std::cout << " by state: "<< m_nodes_tuples[ tuple_idx ] << "" ;
						std::cout << std::endl;
#endif
					}
					else
						{		
#ifdef DEBUG		
							std::cout<<"\t TUPLE COVERED: ";
							for(unsigned i = 0; i < arity; i++){
								std::cout<< m_strips_model.fluents()[ tuple[i] ]->signature()<<"  ";
							}
				
							std::cout << " by state: "<< m_nodes_tuples[ tuple_idx ] << "" <<std::flush;
								
							std::cout<< std::endl;
#endif
						}

				}
			}
		return new_covers;
	}


	bool cover_tuples( const State& s, unsigned arity  )
	{


		bool new_covers = false;

		std::vector<unsigned> tuple( arity );

		unsigned n_combinations = pow( s.fluent_vec().size() , arity );


#ifdef DEBUG

		std::cout<< s << " covers: " << std::endl;
#endif
		for( unsigned idx = 0; idx < n_combinations; idx++ ){
			/**
			 * get tuples from indexes
			 */
			idx2tuple( tuple, s, idx, arity );

			/**
			 * Check if tuple is covered
			 */
			unsigned tuple_idx = tuple2idx( tuple, arity );

			/**
			 * new_tuple if
			 * -> none was registered
			 * OR
			 * -> n better than old_n
			 */
			bool cover_new_tuple = ( !m_nodes_tuples[ tuple_idx ] ) ? true : ( is_better( m_nodes_tuples[tuple_idx], s  ) ? true : false);
			
			if( cover_new_tuple ){
				m_nodes_tuples[ tuple_idx ] = (State*) &s;

				new_covers = true;
#ifdef DEBUG
				std::cout<<"\t";
				for(unsigned i = 0; i < arity; i++){
					std::cout<< m_strips_model.fluents()[ tuple[i] ]->signature()<<"  ";
				}
				std::cout << std::endl;
#endif
			}

		}

		return new_covers;

	}


	inline unsigned  tuple2idx( std::vector<unsigned>& indexes, unsigned arity) const
	{
		unsigned idx=0;
		unsigned dimension = 1;

		for(int i = arity-1; i >= 0; i--)
			{
				idx += indexes[ i ] * dimension;
				dimension*= m_num_fluents;
			}

		return idx;

	}

	inline void      idx2tuple( std::vector<unsigned>& tuple, const State& s, unsigned idx, unsigned arity ) const
	{
		unsigned next_idx, div;
		unsigned current_idx = idx;
		unsigned n_atoms =  s.fluent_vec().size();

		for(unsigned i = arity-1; i >= 0 ; i--){
			div = pow( n_atoms , i );
			next_idx = current_idx % div;
			// if current_idx is zero and is the last index, then take next_idx
			current_idx = ( current_idx / div != 0 || i != 0) ? current_idx / div : next_idx;

			tuple[ i ] = s.fluent_vec()[ current_idx ];

			current_idx = next_idx;
			if(i == 0) break;
		}
	}

	inline bool      is_better( State* s,const State& new_s ) const { return false; }


	const STRIPS_Problem&	m_strips_model;
        std::vector<State*>     m_nodes_tuples;
        unsigned                m_arity;
	unsigned long           m_num_tuples;
	unsigned                m_num_fluents;
	unsigned                m_max_memory_size_MB;
};


}

}

#endif // novelty.hxx
