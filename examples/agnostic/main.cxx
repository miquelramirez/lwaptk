// MRJ: In this example, I'll try to show how can one assemble a simple
// STRIPS planning problem from objects and data structures from an application.
#include <strips_prob.hxx>
#include <fluent.hxx>
#include <action.hxx>
#include <cond_eff.hxx>
#include <string>
#include <vector>
#include <map>
#include <sstream>

// MRJ: Extremely simple graph data structure
class Graph {
public:

	class Vertex {
	public:
		Vertex( unsigned index, std::string l ) : m_index( index ), m_label(l) {}
		~Vertex() {}

		unsigned			index() const { return m_index; }
		std::string			label() const { return m_label; }
		std::vector<Vertex*>& 		neighbours() { return m_neighbours; }

	protected:
		unsigned		m_index;
		std::string		m_label;
		std::vector< Vertex* >	m_neighbours;
	};

	Graph() {
	}

	~Graph() {
	}

	void	add_vertex( std::string label ) {
		Vertex* v = new Vertex( m_vertices.size(), label );
		m_vertices.push_back( v );
	}

	void	connect( unsigned v1, unsigned v2 ) {
		m_vertices.at(v1)->neighbours().push_back( m_vertices.at(v2) );
		m_vertices.at(v2)->neighbours().push_back( m_vertices.at(v1) );
	}
	
	std::vector< Vertex* >&	vertices() { return m_vertices; }

	typedef	std::vector<Vertex*>::iterator	Vertex_It;

	// MRJ: Iterator interface to access vertices adjacent
	Vertex_It	begin_adj( unsigned v1 ) { return m_vertices.at(v1)->neighbours().begin(); }
	Vertex_It	end_adj( unsigned v1 ) { return m_vertices.at(v1)->neighbours().end(); }

protected:

	std::vector< Vertex* >  		m_vertices;
};

int main( int argc, char** argv ) {

	// MRJ: We create our graph
	Graph	g;

	g.add_vertex( "Kitchen" );	// v0
	g.add_vertex( "Sitting_Room" );	// v1
	g.add_vertex( "Balcony" );	// v2
	g.add_vertex( "Bathroom" );	// v3
	g.add_vertex( "Bedroom" );	// v4

	g.connect( 0, 1 );
	g.connect( 1, 2 );
	g.connect( 1, 3 );
	g.connect( 1, 4 );

	aptk::STRIPS_Problem prob;

	// MRJ: Now we create one fluent for each of the possible locations, and keep
	// the indices corresponding to each fluent associated with the appropiate
	// graph vertex
	std::map<unsigned, unsigned>	vtx_to_fl;

	for ( unsigned v_k = 0; v_k < g.vertices().size(); v_k++ ) {
		std::stringstream buffer;
		buffer << "(at " << g.vertices()[v_k]->label() << ")";
		unsigned fl_idx = aptk::STRIPS_Problem::add_fluent( prob, buffer.str() );
		vtx_to_fl.insert( std::make_pair( v_k, fl_idx ) );
	}

	// MRJ: Actions in this task correspond to (directed) edges in the graph
	for ( unsigned v_k = 0; v_k < g.vertices().size(); v_k++ )
		for ( Graph::Vertex_It v_j = g.begin_adj( v_k );
			v_j != g.end_adj( v_k ); v_j++ ) {

			aptk::Fluent_Vec pre; // Precondition
			aptk::Fluent_Vec add; // Adds
			aptk::Fluent_Vec del; // Dels
			aptk::Conditional_Effect_Vec ceff; // Conditional effects
			std::stringstream buffer; // Buffer to build the signature
			
			buffer << "(move " << g.vertices()[v_k]->label() << " " << (*v_j)->label() << ")";

			// MRJ: actions have as a precondition that agent is at location v_k
			pre.push_back( vtx_to_fl[ v_k ] );
			// MRJ: once the action is executed, the agent will now be at v_j
			add.push_back( vtx_to_fl[ (*v_j)->index() ] );
			// MRJ: and no longer where it was
			del.push_back( vtx_to_fl[ v_k ] );

			// MRJ: now we can register the action, but in this example we don't
			// need to keep a reference to the action which results of this
			aptk::STRIPS_Problem::add_action( prob, buffer.str(), pre, add, del, ceff );
		}

	// MRJ: After adding actions, it is necessary to initialize data structures that
	// keep track of relationships between fluents and actions. These data structures
	// are used to improve efficiency of search and heuristic computations.

	prob.make_action_tables();

	// MRJ: Now we can specify the initial and goal states
	aptk::Fluent_Vec	I;
	aptk::Fluent_Vec	G;
	
	// MRJ: The agent starts at the kitchen
	I.push_back( vtx_to_fl[ 0 ] );
	// MRJ: And wants to get to the balcony
	G.push_back( vtx_to_fl[ 2 ] );

	// MRJ: And now we set the initial and goal states of prob	
	aptk::STRIPS_Problem::set_init( prob, I );
	aptk::STRIPS_Problem::set_goal( prob, G );

	// MRJ: We'd be ready for solving this problem with a search algorithm
	// but that's stuff to be covered by another example

	return 0;
}