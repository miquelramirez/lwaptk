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

#ifndef __H_2__
#define __H_2__

#include <aptk/search_prob.hxx>
#include <aptk/heuristic.hxx>
#include <aptk/bit_set.hxx>
#include <strips_state.hxx>
#include <strips_prob.hxx>
#include <vector>
#include <iosfwd>

namespace aptk {

namespace agnostic {

namespace H2_Helper {
	
	inline int	pair_index( unsigned p, unsigned q ) {
		return ( p >= q ? p*(p+1)/2 + q : q*(q+1)/2 + p);
	}

}

enum class H2_Cost_Function { Zero_Costs, Unit_Costs, Use_Costs };

template <typename Search_Model, H2_Cost_Function cost_opt = H2_Cost_Function::Use_Costs >
class H2_Heuristic : public Heuristic<State> {

public:
	H2_Heuristic( const Search_Model& prob )
	: Heuristic<State>( prob ), m_strips_model( prob.task() ) {
		unsigned F = m_strips_model.num_fluents();
		m_values.resize( (F*F + F)/2 );
		m_op_values.resize( m_strips_model.num_actions() );
		m_interfering_ops.resize( F );
		for ( unsigned p = 0; p < m_interfering_ops.size(); p++ ) {
			m_interfering_ops[p] = new Bit_Set( m_strips_model.num_actions() );
			for ( unsigned op = 0; op < m_strips_model.num_actions(); op++ ) 	{
				const Action* op_ptr = m_strips_model.actions()[op];
				if ( op_ptr->add_set().isset( p ) || op_ptr->del_set().isset( p ) )
					m_interfering_ops[p]->set(op);
			}
		}	
	}

	virtual ~H2_Heuristic() {
	}

	virtual	void	eval( const State& s, float& h_val ) {
		initialize( s );
		compute();
		h_val = eval( m_strips_model.goal() ); 
	}

	virtual void eval( const State& s, float& h_val,  std::vector<Action_Idx>& pref_ops ) {
		eval( s, h_val );
	}

	float&	op_value( unsigned a ) 		{ return m_op_values.at(a); }
	float   op_value( unsigned a ) const 	{ return m_op_values.at(a); }

	float& value( unsigned p, unsigned q ) 	{
		assert( H2_Helper::pair_index(p,q) < (int)m_values.size() );
		return m_values[H2_Helper::pair_index(p,q)];
	}

	float value( unsigned p, unsigned q ) const 	{
		assert( H2_Helper::pair_index(p,q) < (int)m_values.size() );
		return m_values[H2_Helper::pair_index(p,q)];
	}

	float& value( unsigned p ) 	{
		assert( H2_Helper::pair_index(p,p) < (int)m_values.size() );
		return m_values[H2_Helper::pair_index(p,p)];
	}

	float value( unsigned p ) const 	{
		assert( H2_Helper::pair_index(p,p) < (int)m_values.size() );
		return m_values[H2_Helper::pair_index(p,p)];
	}

	float eval( const Fluent_Vec& s ) const {
		float v = 0;
		for ( unsigned i = 0; i < s.size(); i++ )
			for ( unsigned j = i; j < s.size(); j++ ){			
				v = std::max( v, value( s[i], s[j] ) );		
				if(v == infty ) return infty;
			}

		return v;
	}

	bool  is_mutex( const Fluent_Vec& s ) const {
		return eval(s) == infty;
	}

	bool	is_mutex( unsigned p, unsigned q ) const {
		return value( p, q ) == infty;
	}

	float eval( const Fluent_Vec& s, unsigned p ) const {
		float v = 0;
		for ( unsigned k = 0; k < s.size(); k++ )
			v = std::max( v, value( s[k], p ) );
		float v2 = 0;
		for ( unsigned i = 0; i < s.size(); i++ )
			for ( unsigned j = i; j < s.size(); j++ )
				v2 = std::max( v2, value( s[i], s[j] ) );
		return std::max( v, v2 );
	}

	bool interferes( unsigned a, unsigned p ) const {
		return m_interfering_ops[p]->isset(a);
	}

	void print_values( std::ostream& os ) const {
		for ( unsigned p = 0; p < m_strips_model.fluents().size(); p++ )
			for ( unsigned q = p; q < m_strips_model.fluents().size(); q++ ) {
				os << "h²({ ";
				os << m_strips_model.fluents()[p]->signature();
				os << ", ";
				os << m_strips_model.fluents()[q]->signature();
				os << "}) = " << value(p,q) << std::endl;
			}		
	}

	void compute_edeletes( STRIPS_Problem& prob ){

		initialize( prob.init() );
		compute_mutexes_only();


		for ( unsigned p = 0 ; p < prob.num_fluents(); p++ ){
			for ( unsigned a = 0; a < prob.num_actions(); a++ ){
				bool is_edelete = false;
				Action& action = *(prob.actions()[a]);

				for ( unsigned i = 0; i < action.add_vec().size(); i++ ){
					unsigned q = action.add_vec()[i];
					if ( value(p,q) == infty ){
						is_edelete = true;
						action.edel_vec().push_back( p );					
						action.edel_set().set( p );
						prob.actions_edeleting( p ).push_back( (const Action*) &action );
						break;
					}
				}

				if ( is_edelete ) continue;

				for ( unsigned i = 0; i < action.prec_vec().size(); i++ ){
					unsigned r = action.prec_vec()[i];
					if ( !action.add_set().isset(p) && value( p, r ) == infty ){
						action.edel_vec().push_back( p );
						action.edel_set().set( p );
						prob.actions_edeleting( p ).push_back( (const Action*) &action );
						break;
					}
				}

				if ( !action.edel_set().isset(p) && action.del_set().isset(p) ){
					action.edel_vec().push_back( p );
					action.edel_set().set( p );
					prob.actions_edeleting( p ).push_back( (const Action*) &action );
				}

			}
		}
		
#ifdef DEBUG
		print_values(std::cout);
		prob.print_actions(std::cout);
#endif
	}

protected:

	void initialize( const State& s ) {
		for ( unsigned k = 0; k < m_values.size(); k++ )
			m_values[k] = infty;
		for ( unsigned k = 0; k < m_op_values.size(); k++ )
			m_op_values[k] = infty;
		for ( unsigned i = 0; i < s.fluent_vec().size(); i++ )
		{
			unsigned p = s.fluent_vec()[i];
			value(p,p) = 0.0f;
			for ( unsigned j = i+1; j < s.fluent_vec().size(); j++ )
			{
				unsigned q = s.fluent_vec()[j];
				value(p,q) = 0.0f;
			}
		}
	}

	void initialize( const Fluent_Vec& f ) {
		for ( unsigned k = 0; k < m_values.size(); k++ )
			m_values[k] = infty;
		for ( unsigned k = 0; k < m_op_values.size(); k++ )
			m_op_values[k] = infty;
		for ( unsigned i = 0; i < f.size(); i++ )
		{
			unsigned p = f[i];
			value(p,p) = 0.0f;
			for ( unsigned j = i+1; j < f.size(); j++ )
			{
				unsigned q = f[j];
				value(p,q) = 0.0f;
			}
		}
	}

	void compute() {
		bool fixed_point;
		
		do {
			fixed_point = true;

			for ( unsigned a = 0; a < m_strips_model.num_actions(); a++ ) {
				const Action& action = *(m_strips_model.actions()[a]);
				op_value(a) = eval( action.prec_vec() );
				if ( op_value(a) == infty ) continue;
				
				for ( unsigned i = 0; i < action.add_vec().size(); i++ ) {
					unsigned p = action.add_vec()[i];
					for ( unsigned j = i; j < action.add_vec().size(); j++ ) {
						unsigned q = action.add_vec()[j];
						float curr_value = value(p,q);
						if ( curr_value == 0.0f ) continue;
						float v = op_value(a);
						if ( cost_opt == H2_Cost_Function::Unit_Costs ) v += 1.0f;
						if ( cost_opt == H2_Cost_Function::Use_Costs ) v += action.cost();
						if ( v < curr_value ) {
							value(p,q) = v;
							fixed_point = false;
						}
					}

					for ( unsigned r = 0; r < m_strips_model.num_fluents(); r++ ) {
						if ( interferes( a, r ) || value( p, r ) == 0.0f ) continue;
						float h2_pre_noop = std::max( op_value(a), value(r,r) );
						if ( h2_pre_noop == infty ) continue;
						for ( unsigned j = 0; j < action.prec_vec().size(); j++ ) {
							unsigned s = action.prec_vec()[j];
							h2_pre_noop = std::max( h2_pre_noop, value(r,s) );
						}
						float v = h2_pre_noop;
						if ( cost_opt == H2_Cost_Function::Unit_Costs ) v += 1.0f;
						if ( cost_opt == H2_Cost_Function::Use_Costs ) v += action.cost();
						if ( v < value(p,r) )
						{
							value(p,r) = v;
							fixed_point = false;
						}													
					}

				}
			}
			
		} while ( !fixed_point );
		
	}


	void compute_mutexes_only() {
		bool fixed_point;
		
		do {
			fixed_point = true;

			for ( unsigned a = 0; a < m_strips_model.num_actions(); a++ ) {
				const Action& action = *(m_strips_model.actions()[a]);
				op_value(a) = eval( action.prec_vec() );
				if ( op_value(a) == infty ) continue;
				
				for ( unsigned i = 0; i < action.add_vec().size(); i++ ) {
					unsigned p = action.add_vec()[i];
					for ( unsigned j = i; j < action.add_vec().size(); j++ ) {
						unsigned q = action.add_vec()[j];
						float curr_value = value(p,q);
						if ( curr_value == 0.0f ) continue;
						value(p,q) = 0.0f;
						fixed_point = false;
							
					}

					for ( unsigned r = 0; r < m_strips_model.num_fluents(); r++ ) {
						if ( interferes( a, r ) || value( p, r ) == 0.0f ) continue;
						float h2_pre_noop = std::max( op_value(a), value(r,r) );
						if ( h2_pre_noop == infty ) continue;
						
						for ( unsigned j = 0; j < action.prec_vec().size(); j++ ) {
							unsigned s = action.prec_vec()[j];
							h2_pre_noop = std::max( h2_pre_noop, value(r,s) );
							if ( h2_pre_noop == infty ) break;
						}
						
						if ( h2_pre_noop == infty ) continue;
						value(p,r) = 0.0f;
						fixed_point = false;
																			
					}

				}
			}
			
		} while ( !fixed_point );
		
	}
		


protected:
	
	const STRIPS_Problem&			m_strips_model;
	std::vector<float>			m_values;
	std::vector<float>			m_op_values;
	std::vector< Bit_Set* >			m_interfering_ops;
};

}

}
#endif // h_2.hxx
