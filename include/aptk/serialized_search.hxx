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

#ifndef __SERIALIZED_SEARCH__
#define __SERIALIZED_SEARCH__

#include <aptk/search_prob.hxx>
#include <aptk/resources_control.hxx>
#include <aptk/closed_list.hxx>
#include <aptk/iw.hxx>
#include <h_1.hxx>
#include <vector>
#include <algorithm>
#include <iostream>

namespace aptk {

namespace search {


template < typename Search_Model, typename Search_Strategy, typename Search_Node >
class Serialized_Search : public Search_Strategy {

public:

	typedef		typename Search_Model::State_Type		                          State;
	typedef 	Closed_List< Search_Node >			                          Closed_List_Type;
	typedef         aptk::agnostic::H1_Heuristic< Search_Model,aptk::agnostic::H_Max_Evaluation_Function > H1_Reachability;	

	Serialized_Search( 	const Search_Model& search_problem ) 
	: Search_Strategy( search_problem ), m_reachability( search_problem ) {	   	
	}

	virtual ~Serialized_Search() {
	}

	void debug_info( State*s, Fluent_Vec& unachieved ){
			
			std::cout << std::endl;
			std::cout << "Goals Achieved: ";


			for(Fluent_Vec::iterator it = m_goals_achieved.begin(); it != m_goals_achieved.end(); it++){
				std::cout << " " << this->problem().task().fluents()[ *it ]->signature(); 
			}
			
			std::cout << std::endl;
			std::cout << "Unachieved Goals: ";
			for(Fluent_Vec::iterator it = unachieved.begin(); it != unachieved.end(); it++){
				std::cout << " " << this->problem().task().fluents()[ *it ]->signature(); 
			}
			std::cout << std::endl;
			std::cout << "Current State: ";
			s->print( std::cout );

	}

	virtual bool  is_goal( State* s ){

		for(Fluent_Vec::iterator it =  m_goals_achieved.begin(); it != m_goals_achieved.end(); it++){
			if(  ! s->entails( *it ) )
				return false;
					
		}
		
		

		
		bool new_goal_achieved = false; 
		Fluent_Vec unachieved;
		for(Fluent_Vec::iterator it = m_goal_candidates.begin(); it != m_goal_candidates.end(); it++){
			if(  s->entails( *it ) )
				{
					m_goals_achieved.push_back( *it );		
					
					float val =  0.0f;
					m_reachability.eval_reachability( *s, val, &m_goals_achieved );

					//					debug_info( s, unachieved );

					if(val != infty) 		
						new_goal_achieved = true;
					else{	
						unachieved.push_back( *it );
						m_goals_achieved.pop_back();
					}
					
				}
			else
				unachieved.push_back( *it );
		}
		
		if ( new_goal_achieved ){
			m_goal_candidates = unachieved;			
			return true;
		}
		else
			return false;	

	}
	
	virtual bool	find_solution( float& cost, std::vector<Action_Idx>& plan ) {
		
		Search_Node* end = NULL;
		State* new_init_state = NULL;
		m_goals_achieved.clear();
		m_goal_candidates.clear();
		m_goal_candidates.insert( m_goal_candidates.begin(), 
					  this->problem().task().goal().begin(), this->problem().task().goal().end() );

		do{
			end = this->do_search();		

			if ( end == NULL ) return false;

			std::vector<Action_Idx> partial_plan;
			this->extract_plan( this->m_root, end, partial_plan, cost );	
			plan.insert( plan.end(), partial_plan.begin(), partial_plan.end() );			

			new_init_state = new State( this->problem().task() );
			new_init_state->set( end->state()->fluent_vec() );
			new_init_state->update_hash();
			this->start( new_init_state );


		}while( !this->problem().goal( *new_init_state ) );


		
		return true;
	}


	
protected:	       
	H1_Reachability                         m_reachability;	

	Fluent_Vec                              m_goals_achieved;
	Fluent_Vec                              m_goal_candidates;
};

}

}



#endif // serialized_search.hxx
