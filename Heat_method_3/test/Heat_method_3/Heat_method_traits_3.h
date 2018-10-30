
struct Heat_method_traits_3
{
  typedef double FT;
  
  struct Point_3 {
    Point_3()
    {}
    
    Point_3(double, double,double)
    {}
    Point_3(const Point_3&)
    {}
  };
  

  struct Vector_3 {
    Vector_3()
    {}

    Vector_3(double, double, double)
    {}
    Vector_3(const Vector_3&)
    {}


  };
  

  struct Point_2 {
    Point_2()
    {}
    
    Point_2(double, double)
    {}
    Point_2(const Point_2&)
    {}
  };

  struct Compute_x_2 {
    double operator()(const Point_2&)const
    {return 0;}
  };
  
  struct Compute_y_2 {
    double operator()(const Point_2&)const
    {return 0;}
  };
  
  
  struct Construct_vector_3{
    Vector_3 operator()(const Point_3&, const Point_3&) const
    { return Vector_3();}
  };
  
  struct Construct_scaled_vector_3{
    Vector_3 operator()(const Vector_3&, double) const
    { return Vector_3();}
  };
    
  struct Construct_cross_product_vector_3 {
  
    Vector_3 operator()(const Vector_3&, const Vector_3&) const
          { return Vector_3();}
  };
   
  struct Construct_sum_of_vectors_3 {
  
    Vector_3 operator()(const Vector_3&, const Vector_3&) const
          { return Vector_3();}
  };
  struct Compute_scalar_product_3 {
  
    double operator()(const Vector_3&, const Vector_3&) const
    { return 0;}
  };

  struct Compute_squared_distance_3 {
  
    double operator()(const Point_3&, const Point_3&) const
    { return 0;}
  };
  
  Compute_x_2 compute_x_2_object() const 
  {
    return Compute_x_2();
  }

  Compute_y_2 compute_y_2_object() const 
  {
    return Compute_y_2();
  }

  
  Construct_vector_3 construct_vector_3_object() const
  {
    return Construct_vector_3();
  }
  Construct_scaled_vector_3 construct_scaled_vector_3_object() const
  {
    return Construct_scaled_vector_3();
  }
  
  Construct_cross_product_vector_3 construct_cross_product_vector_3_object() const
  {
    return Construct_cross_product_vector_3();
  }
  
  Compute_squared_distance_3 compute_squared_distance_3_object() const
  {
    return Compute_squared_distance_3();
  }
  
  Compute_scalar_product_3 compute_scalar_product_3_object() const
  {
    return Compute_scalar_product_3();
  }
  
  Construct_sum_of_vectors_3 construct_sum_of_vectors_3_object() const
  {
    return Construct_sum_of_vectors_3();
  }
};
