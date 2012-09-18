#ifndef __ANYTIME_MULTIPLE_QUEUE_MULTIPLE_HEURISTIC_RESTARTING_WEIGHTED_BEST_FIRST_SEARCH__
#define __ANYTIME_MULTIPLE_QUEUE_MULTIPLE_HEURISTIC_RESTARTING_WEIGHTED_BEST_FIRST_SEARCH__

#include <aptk/search_prob.hxx>
#include <aptk/resources_control.hxx>
#include <aptk/closed_list.hxx>
#include <vector>
#include <algorithm>
#include <iostream>
#include <aptk/at_bfs_dq_mh.hxx>

namespace aptk {

namespace search {

namespace bfs_dq_mh {

// Anytime Restarting Weighted Best-First Search, with two open lists - one for preferred operators, the other for non-preferred
// operator, and one single heuristic estimator, with delayed evaluation of states generated.
// Evaluation function f(n) corresponds to that of Weighted A*
//          f(n) = g(n) + W h(n)
// the value of W decreases each time a solution is found, according to the
// value of the decay parameter.
// 
// Details and rationale on how to handle restarts to improve quality of plans found by anytime heuristic search
// can be found on the paper:
//
// The Joy of Forgetting: Faster Anytime Search via Restarting
// Richter, S. and Thayer, J. T. and Ruml, W.
// ICAPS 2010


template <typename Search_Model, typename Primary_Heuristic, typename Secondary_Heuristic, typename Open_List_Type >
class AT_RWBFS_DQ_MH  : public AT_BFS_DQ_MH<Search_Model, Primary_Heuristic, Secondary_Heuristic, Open_List_Type > {

public:
	typedef		typename Search_Model::State_Type		State;
	typedef  	Node< State >					Search_Node;
	typedef 	Closed_List< Search_Node >			Closed_List_Type;

	AT_RWBFS_DQ_MH( 	const Search_Model& search_problem, float W = 5.0f, float decay = 0.75f ) 
	: AT_BFS_DQ_MH<Search_Model, Primary_Heuristic, Secondary_Heuristic, Open_List_Type>(search_problem), m_W( W ), m_decay( decay ) {
	}

	virtual ~AT_RWBFS_DQ_MH() {
		for ( typename Closed_List_Type::iterator i = m_seen.begin();
			i != m_seen.end(); i++ ) {
			assert( this->closed().retrieve( i->second ) == NULL );
			delete i->second;
		}
		m_seen.clear();
	}

	virtual void			eval( Search_Node* candidate ) {
		if ( candidate->seen() ) return;

		std::vector<Action_Idx>	po;
		this->h1().eval( *(candidate->state()), candidate->h1n(), po );
		for ( unsigned k = 0; k < po.size(); k++ )
			candidate->add_po_1( po[k] );	

		po.clear();

		this->h2().eval( *(candidate->state()), candidate->h2n(), po );
		for ( unsigned k = 0; k < po.size(); k++ )
			candidate->add_po_2( po[k] );	
	}

	virtual void 			process(  Search_Node *head ) {

		typedef typename Search_Model::Action_Iterator Iterator;
		Iterator it( this->problem() );
		int a = it.start( *(head->state()) );
		while ( a != no_op ) {
			State *succ = this->problem().next( *(head->state()), a );
			Search_Node* n = new Search_Node( succ, this->problem().cost( *(head->state()), a ), a, head, this->problem().num_actions() );

			if ( is_closed( n ) ) {
				delete n;
				a = it.next();
				continue;
			}

			if ( is_open( n ) ) {
				delete n;
				a = it.next();
				continue;
			}
			if ( is_seen( n ) ) {
				delete n;
				a = it.next();
				continue;
			}
			n->h1n() = head->h1n();
			n->h2n() = head->h2n();
			n->fn() = m_W * n->h1n() + n->gn();

			this->open_node(n, head->is_po_1(a), head->is_po_2(a));

			a = it.next();
		}
		this->inc_eval();
	}

	virtual Search_Node*	 	do_search() {
		Search_Node *head = this->get_node();
		while(head) {
			if ( head->gn() >= this->bound() )  {
				this->inc_pruned_bound();
				this->close(head);
				head = this->get_node();
				continue;
			}

			if(this->problem().goal(*(head->state()))) {
				this->close(head);
				this->set_bound( head->gn() );
				m_W *= m_decay;
				if ( m_W < 1.0f ) m_W = 1.0f;
				restart_search();	
				return head;
			}
			float t = time_used();
			if ( ( t - this->t0() ) > this->time_budget() ) {
				return NULL;
			}	

			this->eval( head );

			this->process(head);
			this->close(head);
			head = this->get_node();
		}
		return NULL;
	}

	void	restart_search() {
		// MRJ: Move Closed to Seen
		for ( typename Closed_List_Type::iterator it = this->closed().begin();
			it != this->closed().end(); it++ ) {
			it->second->set_seen();
			if ( it->second == this->root() ) continue;
			Search_Node* n2 = m_seen.retrieve( it->second );
			if ( n2 == NULL ) {
				m_seen.put( it->second );
				continue;
			}
			if ( n2->gn() <= it->second->gn() ) {
				delete it->second;
				continue;
			}
			m_seen.erase( m_seen.retrieve_iterator( n2 ) );
			m_seen.put( it->second );
		} 
		this->closed().clear();
		// MRJ: Clear the contents of Open
		this->open_hash().clear();
		Search_Node *head = this->get_node();
		while ( head ) {
			delete head;
			head = this->get_node();
		}
		open_node( this->root(), false, false );
	}

	virtual bool is_open( Search_Node *n ) {
		Search_Node *n2 = NULL;

		if( ( n2 = this->open_hash().retrieve(n)) ) {
			
			if(n->gn() < n2->gn())
			{
				n2->m_parent = n->m_parent;
				n2->m_action = n->m_action;
				n2->m_g = n->m_g;
				n2->m_f = m_W * n2->m_h1 + n2->m_g;
				this->inc_replaced_open();
			}
			return true;
		}

		return false;
	}

	bool is_seen( Search_Node* n ) {
		Search_Node* n2 = m_seen.retrieve(n);
		
		if ( n2 == NULL ) return false;
		
		if ( n->gn() < n2->gn() ) {
			n2->gn() = n->gn();
			n2->m_parent = n->m_parent;
			n2->m_action = n->action();
		}
		n2->fn() = m_W * n2->h1n() + n2->gn();
		m_seen.erase( m_seen.retrieve_iterator(n2) );
		this->open_node( n2, n2->parent()->is_po_1(n2->action()), n2->parent()->is_po_2(n2->action()) );
		return true;
	}

protected:

	float					m_W;
	float					m_decay;
	Closed_List_Type			m_seen;	
};

}

}

}

#endif // at_bfs_sq_sh.hxx
