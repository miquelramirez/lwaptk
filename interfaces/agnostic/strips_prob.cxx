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

#include <strips_prob.hxx>
#include <action.hxx>
#include <fluent.hxx>
#include <cassert>
#include <map>
#include <iostream>

namespace aptk
{

	STRIPS_Problem::STRIPS_Problem( std::string dom_name, std::string prob_name )
		: m_domain_name(dom_name), m_problem_name( prob_name ), 
		m_num_fluents( 0 ), m_num_actions( 0 ), m_end_operator_id( no_such_index ),
		m_succ_gen( *this )
	{
	}

	STRIPS_Problem::~STRIPS_Problem()
	{
	}

	void	STRIPS_Problem::make_action_tables()
	{
		m_requiring.resize( fluents().size() );
		m_deleting.resize( fluents().size() );
		m_edeleting.resize( fluents().size() );
		m_adding.resize( fluents().size() );
		m_ceffs_adding.resize( fluents().size() );
		
		for ( unsigned k = 0; k < actions().size(); k++ )
			register_action_in_tables( actions()[k] );
		
		m_succ_gen.build();
	}

	void	STRIPS_Problem::register_action_in_tables( Action* a )
	{
		if ( a->prec_vec().empty() ) {
			m_empty_precs.push_back(a);
		}
		else {
			for ( unsigned k = 0; k < a->prec_vec().size(); k++ )
				actions_requiring(a->prec_vec()[k]).push_back( a );
		}
		for ( unsigned k = 0; k < a->add_vec().size(); k++ )
			actions_adding(a->add_vec()[k]).push_back(a);

		for ( unsigned k = 0; k < a->ceff_vec().size(); k++ )
			for ( unsigned i = 0; i < a->ceff_vec()[k]->add_vec().size(); i++ )
				ceffs_adding( a->ceff_vec()[k]->add_vec()[i] ).push_back( std::make_pair( k, a ) );

		for ( unsigned k = 0; k < a->del_vec().size(); k++ )
			actions_deleting(a->del_vec()[k]).push_back(a);	
		
		//register conditional effects
		/*
		for ( unsigned i = 0; i < a->ceff_vec().size(); i++ )
		{
			for ( unsigned k = 0; k < a->ceff_vec()[i]->prec_vec().size(); k++ )
				actions_requiring(a->ceff_vec()[i]->prec_vec()[k]).push_back( a );
			for ( unsigned k = 0; k < a->ceff_vec()[i]->add_vec().size(); k++ )
				actions_adding(a->ceff_vec()[i]->add_vec()[k]).push_back(a);
			for ( unsigned k = 0; k < a->ceff_vec()[i]->del_vec().size(); k++ )
				actions_deleting(a->ceff_vec()[i]->del_vec()[k]).push_back(a);	
		}
		*/
	}
	
	unsigned STRIPS_Problem::add_action( STRIPS_Problem& p, std::string signature,
					     Fluent_Vec& pre, Fluent_Vec& add, Fluent_Vec& del,
					     Conditional_Effect_Vec& ceffs, float cost )
	{
		Action* new_act = new Action( p );
		new_act->set_signature( signature );
		new_act->define( pre, add, del, ceffs );
		p.increase_num_actions();
		p.actions().push_back( new_act );
		new_act->set_index( p.actions().size()-1 );
		new_act->set_cost( cost );
		p.m_const_actions.push_back( new_act );
		return p.actions().size()-1;
	}

	unsigned STRIPS_Problem::add_fluent( STRIPS_Problem& p, std::string signature )
	{
		Fluent* new_fluent = new Fluent( p );
		new_fluent->set_index( p.fluents().size() );
		new_fluent->set_signature( signature );
		p.m_fluents_map[signature] = new_fluent->index();
		p.increase_num_fluents();
		p.fluents().push_back( new_fluent );
		p.m_const_fluents.push_back( new_fluent );
		return p.fluents().size()-1;
	}

	void	STRIPS_Problem::set_init( STRIPS_Problem& p, Fluent_Vec& init_vec )
	{
#ifdef DEBUG
		for ( unsigned k = 0; k < init_vec.size(); k++ )
			assert( init_vec[k] < p.num_fluents() );
#endif	
		if ( p.m_in_init.empty() )
			p.m_in_init.resize( p.num_fluents(), false );
		else
			for ( unsigned k = 0; k < p.num_fluents(); k++ )
				p.m_in_init[k] = false;

		p.init().assign( init_vec.begin(), init_vec.end() );

		for ( unsigned k = 0; k < init_vec.size(); k++ )
			p.m_in_init[ init_vec[k] ] = true;
	}

	void	STRIPS_Problem::set_goal( STRIPS_Problem& p, Fluent_Vec& goal_vec, bool create_end_op )
	{
#ifdef DEBUG
		for ( unsigned k = 0; k < goal_vec.size(); k++ )
			assert( goal_vec[k] < p.num_fluents() );
#endif
		if ( p.m_in_goal.empty() )
			p.m_in_goal.resize( p.num_fluents(), false );
		else
			for ( unsigned k = 0; k < p.num_fluents(); k++ )
				p.m_in_goal[k] = false;

		p.goal().assign( goal_vec.begin(), goal_vec.end() );

		for ( unsigned k = 0; k < goal_vec.size(); k++ )
			p.m_in_goal[ goal_vec[k] ] = true;
		
		if ( create_end_op )
		{
			Fluent_Vec dummy;
			Conditional_Effect_Vec dummy_ceffs;
			p.m_end_operator_id = add_action( p, "(END)", goal_vec, dummy, dummy, dummy_ceffs);
			p.actions()[ p.m_end_operator_id ]->set_cost( 0 );
		}
	}

	void STRIPS_Problem::print_fluent_vec(const Fluent_Vec &a) {
		for(unsigned i = 0; i < a.size(); i++) {
			std::cout << fluents()[a[i]]->signature() << ", ";
		}
	}

        unsigned STRIPS_Problem::get_fluent_index( std::string signature ) {
              return m_fluents_map[signature];
        }

	void	STRIPS_Problem::print( std::ostream& os ) const {
		os << "# Fluents: " << num_fluents() << std::endl;
		print_fluents( os );
		os << "# Actions: " << num_actions() << std::endl;
		print_actions( os );
	}

	void	STRIPS_Problem::print_fluents( std::ostream& os ) const {
		for ( unsigned k = 0; k < fluents().size(); k++ ) {
			os << k+1 << ". " << fluents().at(k)->signature() << std::endl;
		}
	}

	void	STRIPS_Problem::print_action( unsigned idx, std::ostream& os ) const {
			actions().at(idx)->print( *this, os );
	}

	void	STRIPS_Problem::print_actions( std::ostream& os ) const {
		os << "Actions" << std::endl;
		for ( unsigned k = 0; k < actions().size(); k++ )
			actions().at(k)->print( *this, os );
	}

	void	STRIPS_Problem::print_fluent_vec( std::ostream& os, const Fluent_Vec& v ) const {
		for ( unsigned k = 0; k < v.size(); k++ )
		{	
			unsigned p = v[k];
			os << fluents()[p]->signature();
			if ( k < v.size()-1 ) os << ", ";
		}		
	}
}
