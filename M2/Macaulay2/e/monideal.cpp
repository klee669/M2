// (c) 1994 Michael E. Stillman

#include "monideal.hpp"
#include "monoid.hpp"
#include "bin_io.hpp"
#include "text_io.hpp"

extern int comp_printlevel;

stash *MonomialIdeal_rec::mystash;
stash *Nmi_node::mystash;
stash *monideal_pair::mystash;

Nmi_node::~Nmi_node()
{
  if (right != header) delete right;
  if (tag == node)
    {
      if (header != this) delete down();
    }
  else
    delete baggage();
}

MonomialIdeal::MonomialIdeal(const Ring *R, queue<Bag *> &elems, queue<Bag *> &rejects)
  : obj(new MonomialIdeal_rec(R))
{
  array< queue<Bag *> *> bins;
  Bag *b, *b1;
  while (elems.remove(b)) 
    {
      int d = varpower::simple_degree(b->monom().raw());
      if (d >= bins.length())
	for (int i=bins.length(); i<=d; i++)
	  bins.append((queue<Bag *> *)NULL);
      if (bins[d] == (queue<Bag *> *)NULL)
	bins[d] = new queue<Bag *>;
      bins[d]->insert(b);
    }
  int n = get_ring()->n_vars();
  intarray exp;
  for (int i=0; i < bins.length(); i++)
    if (bins[i] != NULL)
      {
	while (bins[i]->remove(b))
	  {
	    const int *mon = b->monom().raw();
	    exp.shrink(0);
	    varpower::to_ntuple(n, mon, exp);
	    if (search_expvector(exp.raw(), b1))
	      rejects.insert(b);
	    else
	      insert_minimal(b);
	  }
	delete bins[i];
      }
}

MonomialIdeal::MonomialIdeal(const Ring *R, queue<Bag *> &elems)
  : obj(new MonomialIdeal_rec(R))
{
  array< queue<Bag *> *> bins;
  Bag *b;
  while (elems.remove(b)) 
    {
      int d = varpower::simple_degree(b->monom().raw());
      if (d >= bins.length())
	for (int i=bins.length(); i<=d; i++)
	  bins.append((queue<Bag *> *)NULL);
      if (bins[d] == (queue<Bag *> *)NULL)
	bins[d] = new queue<Bag *>;
      bins[d]->insert(b);
    }
  for (int i=0; i < bins.length(); i++)
    if (bins[i] != NULL)
      {
	while (bins[i]->remove(b)) insert(b);
	delete bins[i];
      }
}

MonomialIdeal::MonomialIdeal(const Ring *R)
  : obj(new MonomialIdeal_rec(R))
{
}

const int *MonomialIdeal::first_elem() const
{
  return first_node()->monom().raw();
}

const int *MonomialIdeal::second_elem() const
{
  return next(first_node())->monom().raw();
}

MonomialIdeal MonomialIdeal::copy() const
{
  MonomialIdeal result(get_ring());
  for(Index<MonomialIdeal> i = first(); i.valid(); i++)
    result.insert_minimal(new Bag(*( operator[](i)  )));
  return result;
}

bool MonomialIdeal::is_equal(const MonomialIdeal &mi) const
{
  if (this == &mi) return true;
  if (length() != mi.length()) return false;
  Index<MonomialIdeal> i = first();
  Index<MonomialIdeal> j = mi.first();
  while (i.valid())
    {
      const int *m = operator[](i)->monom().raw();
      const int *n = mi[j]->monom().raw();
      if (!varpower::is_equal(m, n))
	return false;
      i++;
      j++;
    }
  return true;
}

int MonomialIdeal::search_expvector(const int *exp, Bag *&b) const
{
  if (obj->mi == NULL) return 0;

  Nmi_node *p = obj->mi;

  for (;;)
    {
      p = p->right;

      if (p == p->header)
	{
	  if ((p = p->down()) == NULL) return 0; 
	  continue;
	}
      
      if (p->exp > exp[p->var])
	{
	  if ((p = p->header->down()) == NULL) return 0;
	  continue;
	}

      if (p->tag == Nmi_node::leaf) 
	{
	  b = p->baggage();
	  return 1;
	} 

      p = p->down();	
    }
}

void MonomialIdeal::find_all_divisors(const int *exp, array<Bag *> &b) const
{
  b.shrink(0);
  if (obj->mi == NULL) return;

  Nmi_node *p = obj->mi;

  for (;;)
    {
      p = p->right;

      if (p == p->header)
	{
	  if ((p = p->down()) == NULL) return;
	  continue;
	}
      
      if (p->exp > exp[p->var])
	{
	  if ((p = p->header->down()) == NULL) return;
	  continue;
	}

      if (p->tag == Nmi_node::leaf) 
	{
	  b.append(p->baggage());
	} 
      else
	p = p->down();	
    }
}

int MonomialIdeal::search(const int *m, Bag *&b) const
{
  static intarray exp;
  exp.shrink(0);
  varpower::to_ntuple(get_ring()->n_vars(), m, exp);
  return search_expvector(exp.raw(), b);
}

Nmi_node *MonomialIdeal::next(Nmi_node *p) const
{
  while (p != NULL) 
    {
      p = p->left;
      if (p->tag == Nmi_node::leaf)
	return p;
      else
	p = p->down();
    }
  return NULL;
}

void *MonomialIdeal::next(void *p) const
{
  return (void *) next((Nmi_node *) p);
}

Nmi_node *MonomialIdeal::prev(Nmi_node *p) const
{
  while (p != NULL) 
    {
      p = p->right;
      if (p->tag == Nmi_node::leaf)
	return p;
      else
	p = p->down();
    }
  return NULL;
}

void *MonomialIdeal::prev(void *p) const
{
  return (void *) prev((Nmi_node *) p);
}

void MonomialIdeal::insert1(Nmi_node *&top, Bag *b)
{
  Nmi_node **p = &top, *up = NULL;
  int one_element = 1;

  for (index_varpower i = b->monom().raw(); i.valid();)
    {
      one_element = 0;
      int insert_var = i.var();
      int insert_exp;
      
      if (*p == NULL)
	{
	  // make a new header node
	  *p = new Nmi_node(insert_var, 0, up);
	  (*p)->header = (*p)->left = (*p)->right = *p;
	}
      else if ((*p)->var < insert_var)
	{
	  // make a new layer
	  Nmi_node *header_node, *zero_node;
	  header_node = new Nmi_node(insert_var, 0, up);
	  zero_node   = new Nmi_node(insert_var, 0, *p);

	  header_node->left = header_node->right = zero_node;
	  (*p)->down() = zero_node;
	  *p = header_node->header = zero_node->header 
	    = zero_node->left = zero_node->right = header_node;
	}

      if ((*p)->var > insert_var)
	{ insert_var = (*p)->var; insert_exp = 0; }
      else
	{ insert_exp = i.exponent(); ++i; }

      Nmi_node *q = (*p)->right;
      while((q != q->header) && (q->exp < insert_exp)) q = q->right;
      if (q->exp != insert_exp)
	{
	  Nmi_node *insert_node;

	  if (i.valid())
	    {
	      insert_node
		= new Nmi_node(insert_var, insert_exp,
					(Nmi_node *)NULL);
	      q->insert_to_left(insert_node);
	      q = insert_node;
	    }
	  else
	    {
	      insert_node
		= new Nmi_node(insert_var, insert_exp, b);
	      q->insert_to_left(insert_node);
	      return;
	    }
	}

      up = q;
      p = &(q->down());
    }
  if (one_element)
    {
      // insert a header node and a var/exp = 0/0 leaf
      top = new Nmi_node(0, 0, (Nmi_node *) NULL);
      Nmi_node *leaf_node = new Nmi_node(0, 0, b);
      top->left = top->right = leaf_node;
      top->header = leaf_node->header 
	= leaf_node->left = leaf_node->right = top;
    }
}

void MonomialIdeal::remove1(Nmi_node *p)
{
  assert(p != NULL);
  assert(p->tag == Nmi_node::leaf);
  p->baggage() = NULL;
  obj->count--;

  for(;p != NULL;)
    {
      p->left->right = p->right;
      p->right->left = p->left;
      Nmi_node *q = p->header;
      p->left = p->right = NULL;
      delete p;

      if (q->right == q->header)  // only the header is left, so delete it
	{
	  p = q->down();
	  q->down() = NULL;
	  if (p != NULL) p->down() = NULL;
	  delete q;
	  continue;
	}

      if (q->left != q->right) return;

      if (q->left->exp > 0) return;

      Nmi_node *dad = q->down();
      if (q->left->tag == Nmi_node::leaf)
	{
	  // set parent of q to be a leaf with baggage of q->left
	  // since this is a leaf, dad should be non null
	  assert(dad != NULL);
	  dad->tag = Nmi_node::leaf;
	  dad->baggage() = q->left->baggage();
	}
      else
	{
	  // set parent of q to be node pointing to q->left->down
	  q->left->down()->down() = dad;
	  if (dad != NULL)  
	    dad->down() = q->left->down();
	  else
	    obj->mi = q->left->down();
	  q->left->down() = NULL;
	}
      q->down() = NULL;
      delete q;		// Deletes both nodes q, q->left.
      return;
    }
  if (p == NULL) obj->mi = NULL;
}

int MonomialIdeal::remove(Bag *&b)
{
  Nmi_node *p = (Nmi_node *) next(obj->mi);
  if (p == NULL) return 0;
  b = p->baggage();
  remove1(p);
  return 1;
}

static int nlists = 0;
static int nleaves = 0;
static int nnodes = 0;
static int ndepth = 0;

void MonomialIdeal::do_node(Nmi_node *p, int indent, int disp) const
{
  buffer o;
  int i;
  assert(p->left != NULL);
  assert(p->right != NULL);
  assert(p->left->right == p);
  assert(p->right->left == p);
  if (disp)
    {
      for (i=1; i<=indent; i++) o << ' ';
      o << p->var << ' ' <<  p->exp;
    }
  if (p->tag == Nmi_node::leaf)
    {
      nleaves++;
      if (disp) o << ' ';
      varpower::elem_text_out(o, p->baggage()->monom().raw());
      o << '(';
      o << p->baggage()->basis_elem();
      o << ')';
    }
  else if (p == p->header)
    nlists++;
  else
    nnodes++;
  emit_line(o.str());
}

void MonomialIdeal::do_tree(Nmi_node *p, int depth, int indent, int disp) const
{
  if (depth > ndepth) ndepth = depth;
  do_node(p, indent, disp);
  Nmi_node *q = p->right;
  while (q != p) {
    do_node(q, indent, disp);
    if (q->tag != Nmi_node::leaf)
      do_tree(q->down(), depth+1, indent+2, disp);
    q = q->right;
  }
}

void MonomialIdeal::debug_out(int disp) const
     // Display MonomialIdeal in tree-like form, collect statistics
{
  nlists = 0;
  nnodes = 0;
  nleaves = 0;
  ndepth = 0;
  if (obj->mi != NULL) do_tree(obj->mi, 0, 0, disp);
  buffer o;
  o << "list nodes     = " << nlists << newline;
  o << "internal nodes = " << nnodes << newline;
  o << "monomials      = " << nleaves << newline;
  o << "max depth      = " << ndepth << newline;
  emit(o.str());
}

int MonomialIdeal::debug_check(Nmi_node *p, Nmi_node *up) const
     // Returns the number of leaves at tree with root p.
     // Make sure that the list header is constructed ok, that the 
     // left/right pointers are ok on this level, that the
     // var, exp, values in this train are correct. 
     // Then loop through, checking each node (recursively) and each leaf
{
  Nmi_node *q;
  // First check the node 'p' itself
  assert(p != NULL);
#ifndef NDEBUG
  int v = p->var;
#endif
  assert(v >= 0);
  if (up != NULL) 
    assert(v < up->var);
  assert(p->header == p);
  assert(p->tag == Nmi_node::node);
  assert(p->down() == up);
  assert(p->left != NULL);
  assert(p->right != NULL);

  // Now loop through each element in left/right chain, checking that
  // v, e, left, right values are consistent.
  for (q = p->left; q != p; q = q->left)
    {
      assert(q->left != NULL);
      assert(q->right != NULL);
      assert(q->header == p);
      assert(q->right->left == q);
      assert(q->left->right == q);
      assert(q->var == v);
      assert((q->right == p) || (q->exp < q->right->exp));
      assert(q->exp >= 0);
    }

  // Now loop through again, this time descending into nodes
  int c = 0;
  for (q = p->right; q != p; q = q->right)
    if (q->tag == Nmi_node::node) 
      c += debug_check(q->down(), q);
    else
      c++;
  return c;
}

void MonomialIdeal::debug_check() const
{
  if (obj->count == 0) 
    {
      assert(obj->mi == NULL);
      return;
    }
  assert(obj->mi != NULL);
  assert(debug_check(obj->mi, NULL) == obj->count);
}

int MonomialIdeal::insert(Bag *b)
        // Insert the monomial (and baggage) 'm', if it
	// is not already in the monomial ideal.  Return whether the
	// monomial was actually inserted.  
{
  Bag *old_b;
  const int *m = b->monom().raw();

  if (search(m, old_b)) 
    {
      delete b;
      return 0;
    }
  insert_minimal(b);
  return 1;
}

void MonomialIdeal_rec::text_out(buffer &o) const
{
  MonomialIdeal_rec *I1 = (MonomialIdeal_rec *) this;
  MonomialIdeal I = I1->cast_to_MonomialIdeal();
  I.text_out(o);
}

void MonomialIdeal::text_out(buffer &o) const
{
  int *m = get_ring()->Nmonoms()->make_one();
  for (Index<MonomialIdeal> j = last(); j.valid(); j--)
    {
      const int *n = operator[](j)->monom().raw();
      get_ring()->Nmonoms()->from_varpower(n, m);
      get_ring()->Nmonoms()->elem_text_out(o, m);
      if (comp_printlevel > 0)
	o << '(' << operator[](j)->basis_elem() << ")";
      o << ' ';
    }
  get_ring()->Nmonoms()->remove(m);
}

void MonomialIdeal_rec::bin_out(buffer &o) const
{
  MonomialIdeal_rec *I1 = (MonomialIdeal_rec *) this;
  MonomialIdeal I = I1->cast_to_MonomialIdeal();
  I.bin_out(o);
}

void MonomialIdeal::bin_out(buffer &o) const
{
  int *m = get_ring()->Nmonoms()->make_one();
  bin_int_out(o, obj->count);
  for (Index<MonomialIdeal> i = last(); i.valid(); i--)
    {
      const int *n = operator[](i)->monom().raw();
      get_ring()->Nmonoms()->from_varpower(n, m);
      get_ring()->Nmonoms()->elem_bin_out(o, m);
      bin_int_out(o, operator[](i)->basis_elem());
    }
  get_ring()->Nmonoms()->remove(m);
}

MonomialIdeal MonomialIdeal::intersect(const MonomialIdeal &J) const
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    for (Index<MonomialIdeal> j = J.first(); j.valid(); j++)
      {
	Bag *b = new Bag(operator[](i)->basis_elem());
	varpower::lcm(operator[](i)->monom().raw(), 
		      J[j]->monom().raw(), b->monom());
	new_elems.insert(b);
      }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

MonomialIdeal MonomialIdeal::intersect(const int *m) const
    // Compute (this : m), where m is a varpower monomial.
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = new Bag(operator[](i)->basis_elem());
      varpower::lcm(operator[](i)->monom().raw(), m, b->monom());
      new_elems.insert(b);
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}


MonomialIdeal MonomialIdeal::operator*(const MonomialIdeal &J) const
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    for (Index<MonomialIdeal> j = J.first(); j.valid(); j++)
      {
	Bag *b = new Bag(operator[](i)->basis_elem());
	varpower::mult(operator[](i)->monom().raw(), J[j]->monom().raw(),
		       b->monom());
	new_elems.insert(b);
      }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

MonomialIdeal MonomialIdeal::operator+(const MonomialIdeal &J) const
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = new Bag(  operator[](i) );
      new_elems.insert(b);
    }
  for (Index<MonomialIdeal> j = J.first(); j.valid(); j++)
    {
      Bag *b = new Bag( J[j] );
      new_elems.insert(b);
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

MonomialIdeal MonomialIdeal::operator-(const MonomialIdeal &J) const
    // Create the monomial ideal consisting of those elements of 'this'
    // that are not in 'J'.  The baggage is left the same.
{
  MonomialIdeal result(get_ring());
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *c;
      if (!J.search(operator[](i)->monom().raw(), c))
	  {
	    Bag *b = new Bag(  operator[](i) );
	    result.insert_minimal(b);
	  }
    }
  return result;
}

MonomialIdeal MonomialIdeal::quotient(const int *m) const
    // Compute (this : m), where m is a varpower monomial.
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = new Bag(operator[](i)->basis_elem());
      varpower::divide(operator[](i)->monom().raw(), m, b->monom());
      new_elems.insert(b);
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

MonomialIdeal MonomialIdeal::quotient(const MonomialIdeal &J) const
{
  MonomialIdeal result(get_ring());
  Bag *b = new Bag();
  varpower::one(b->monom());
  result.insert(b);
  for (Index<MonomialIdeal> i = J.first(); i.valid(); i++)
    {
      MonomialIdeal result1 = quotient(operator[](i)->monom().raw());
      result = result.intersect(result1);
    }
  return result;
}

#if 0
MonomialIdeal MonomialIdeal::socle(const MonomialIdeal &J) const
{
  MonomialIdeal result(get_ring());
  for (Index<MonomialIdeal> i = J.first(); i.valid(); i++)
    {
      for (index_varpower j = operator[](i)->monom().raw(); j.valid(); j++)
	{
	  varpower::divide(...);
	  compute m/j
	  if xi (m/j) is in J for all xi
	    {
	      
	      Bag *b = new Bag(vp);
	      result.insert_minimal(b);
	    }
	}
    }
  return result;
}
#endif
MonomialIdeal MonomialIdeal::erase(const int *m) const
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = new Bag(operator[](i)->basis_elem());
      varpower::erase(operator[](i)->monom().raw(), m, b->monom());
      new_elems.insert(b);
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

MonomialIdeal MonomialIdeal::sat(const MonomialIdeal &J) const
{
  MonomialIdeal result(get_ring());
  Bag *b = new Bag();
  varpower::one(b->monom());
  result.insert(b);
  for (Index<MonomialIdeal> i = J.first(); i.valid(); i++)
    {
      MonomialIdeal result1 = erase(operator[](i)->monom().raw());
      result = result.intersect(result1);
    }
  return result;
}

MonomialIdeal MonomialIdeal::radical() const
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = new Bag(  operator[](i)->basis_elem() );
      varpower::radical( operator[](i)->monom().raw(), b->monom() );
      new_elems.insert(b);
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

static void borel1(queue<Bag *> &result, int *m, int loc, int nvars)
{
  if (loc == 0)
    {
      Bag *b = new Bag();
      varpower::from_ntuple(nvars, m, b->monom());
      result.insert(b);
    }
  else
    {
      int a = m[loc];
      for (int i=0; i<=a; i++)
	{
	  borel1(result,m,loc-1,nvars);
	  m[loc]--;
	  m[loc-1]++;
	}
      m[loc] += a + 1;
      m[loc-1] -= a + 1;
    }
}

MonomialIdeal MonomialIdeal::borel() const
    // Return the smallest borel monomial ideal containing 'this'.
{
  queue<Bag *> new_elems;
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = operator[](i);
      intarray bexp;
      varpower::to_ntuple(get_ring()->n_vars(), b->monom().raw(), bexp);
      borel1(new_elems, bexp.raw(),
	     get_ring()->n_vars()-1, get_ring()->n_vars());
    }
  MonomialIdeal result(get_ring(), new_elems);
  return result;
}

int MonomialIdeal::is_borel() const
{
  for (Index<MonomialIdeal> i = first(); i.valid(); i++)
    {
      Bag *b = operator[](i);
      Bag *c;
      intarray bexp;
      varpower::to_ntuple(get_ring()->n_vars(), b->monom().raw(), bexp);
      for (int j=get_ring()->n_vars()-1; j>=1; j--)
	if (bexp[j] > 0)
	  {
	    bexp[j]--;
	    bexp[j-1]++;
	    int isthere = search_expvector(bexp.raw(), c);
	    bexp[j]++;
	    bexp[j-1]--;
	    if (!isthere) return 0;
	  }
    }
  return 1;
}

// Other routines to add:
//   primary_decomposition(J)
//   partition(J)

//   linear versions of: quotient, ...
//   hilbert series
