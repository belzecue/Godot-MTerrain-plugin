#include "mgrid.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <iostream>


MGrid::MGrid(){
    nvec8.append(Vector3(0,0,-1));
    nvec8.append(Vector3(-1,0,-1));
    nvec8.append(Vector3(-1,0,0));
    nvec8.append(Vector3(-1,0,1));
    nvec8.append(Vector3(0,0,1));
    nvec8.append(Vector3(1,0,1));
    nvec8.append(Vector3(1,0,0));
    nvec8.append(Vector3(1,0,-1));
    nvec8.append(Vector3(0,0,-1));
}
MGrid::~MGrid() {
    clear();
}

void MGrid::clear() {
    if(is_dirty){
        RenderingServer* rs = RenderingServer::get_singleton();
        for(int32_t z=_search_bound.top; z <=_search_bound.bottom; z++)
        {
            for(int32_t x=_search_bound.left; x <=_search_bound.right; x++){
                if(points[z][x].has_instance()){
                    rs->free_rid(points[z][x].instance);
                    points[z][x].instance = RID();
                    points[z][x].mesh = RID();
                }
            }
        }
        for(int32_t z=0; z <_size.z; z++){
                memdelete_arr<MPoint>(points[z]);
            }
        memdelete_arr<MPoint*>(points);
        memdelete_arr<MRegion>(regions);
        memdelete(_chunks);
    }
    _size.x = 0;
    _size.y = 0;
    _size.z = 0;
    _grid_bound.clear();
    _search_bound.clear();
    _last_search_bound.clear();
    _region_grid_bound.clear();
    is_dirty = false;
    uniforms_id.clear();
    has_normals = false;
}

bool MGrid::is_created() {
    return is_dirty;
}

MGridPos MGrid::get_size() {
    return _size;
}

void MGrid::set_scenario(RID scenario){
    _scenario = scenario;
}

void MGrid::create(const int32_t& width,const int32_t& height, MChunks* chunks) {
    if (width == 0 || height == 0) return;
    _chunks = chunks;
    _size.x = width;
    _size.z = height;
    // Not added to one because pixels start from zero
    pixel_width = (uint32_t)(_size.x*_chunks->base_size_meter/_chunks->h_scale) + 1;
    pixel_height = (uint32_t)(_size.z*_chunks->base_size_meter/_chunks->h_scale) + 1;
    grid_pixel_region = MPixelRegion(pixel_width,pixel_height);
    _size_meter.x = width*_chunks->base_size_meter;
    _size_meter.z = height*_chunks->base_size_meter;
    _vertex_size.x = (_size_meter.x/chunks->h_scale) + 1;
    _vertex_size.z = (_size_meter.z/chunks->h_scale) + 1;
    _region_grid_size.x = _size.x/region_size + _size.x%region_size;
    _region_grid_size.z = _size.z/region_size + _size.z%region_size;
    _region_grid_bound.right  = _region_grid_size.x - 1;
    _region_grid_bound.bottom = _region_grid_size.z - 1;
    _regions_count = _region_grid_size.x*_region_grid_size.z;
    region_size_meter = region_size*_chunks->base_size_meter;
    rp = (region_size_meter/_chunks->h_scale);
    region_pixel_size = rp + 1;
    _grid_bound = MBound(0,width-1, 0, height-1);
    regions = memnew_arr(MRegion, _regions_count);
    points = memnew_arr(MPoint*, _size.z);
    for (int32_t z=0; z<_size.z; z++){
        points[z] = memnew_arr(MPoint, _size.x);
    }
    //Init Regions
    int index = 0;
    for(int32_t z=0; z<_region_grid_size.z; z++){
        for(int32_t x=0; x<_region_grid_size.x; x++){
            regions[index].grid = this;
            regions[index].pos = MGridPos(x,0,z);
            regions[index].world_pos = get_world_pos(x*region_size,0,z*region_size);
            if(x!=0){
                regions[index].left = get_region(x-1,z);
            }
            if(x!=_region_grid_size.x-1){
                regions[index].right = get_region(x+1,z);
            }
            if(z!=0){
                regions[index].top = get_region(x,z-1);
            }
            if(z!=_region_grid_size.z-1){
                regions[index].bottom = get_region(x,z+1);
            }
            if(_material.is_valid()){
                regions[index].set_material(_material->duplicate());
            }
            index++;
        }
    }
    is_dirty = true;
}

void MGrid::update_regions_uniforms(Array input) {
    ERR_FAIL_COND(!_material.is_valid());
    ERR_FAIL_COND(!_material->get_shader().is_valid());
    for(int i=0;i<input.size();i++){
        Dictionary unifrom_info = input[i];
        update_regions_uniform(unifrom_info);
    }
    for(int i=0; i<_regions_count; i++){
        regions[i].configure();
    }
    if(_regions_count > 0 ){
        for(int i=0;i<regions[0].images.size();i++){
            MImage* img = regions[0].images[i];
            uniforms_id[img->name] = i;
        }
    }
    if(!has_normals){
        generate_normals_thread();
    }
}

void MGrid::update_regions_uniform(Dictionary input) {
    String name = input["name"];
    String uniform_name = input["uniform_name"];
    int compression = input["compression"];
    for(int z=0; z<_region_grid_size.z;z++){
        for(int x=0; x<_region_grid_size.x;x++){
            MRegion* region= get_region(x,z);
            String file_name = name+"_x"+itos(x)+"_y"+itos(z)+".res";
            String file_path = dataDir.path_join(file_name);
            if(!ResourceLoader::get_singleton()->exists(file_path)){
                WARN_PRINT("Can not find "+name);
                return;
            }
            has_normals = name=="normals";
            MImage* i = memnew(MImage(file_path,name,uniform_name,compression));
            region->set_image_info(i);
        }
    }
}


Vector3 MGrid::get_world_pos(const int32_t &x,const int32_t& y,const int32_t& z) {
    return Vector3(x,y,z)*_chunks->base_size_meter + offset;
}

Vector3 MGrid::get_world_pos(const MGridPos& pos){
    return Vector3(pos.x,pos.y,pos.z)*_chunks->base_size_meter + offset;
}

MGridPos MGrid::get_grid_pos(const Vector3& pos) {
    MGridPos p;
    Vector3 rp = pos - offset;
    p.x = ((int32_t)(rp.x))/_chunks->base_size_meter;
    p.y = ((int32_t)(rp.y))/_chunks->base_size_meter;
    p.z = ((int32_t)(rp.z))/_chunks->base_size_meter;
    return p;
}

int32_t MGrid::get_region_id_by_point(const int32_t &x, const int32_t& z) {
    return x/region_size + (z/region_size)*_region_grid_size.x;
}

MRegion* MGrid::get_region_by_point(const int32_t &x, const int32_t& z){
    int32_t id = x/region_size + (z/region_size)*_region_grid_size.x;
    return regions + id;
}

MRegion* MGrid::get_region(const int32_t &x, const int32_t& z){
    int32_t id = x + z*_region_grid_size.x;
    return regions + id;
}

MGridPos MGrid::get_region_pos_by_world_pos(Vector3 world_pos){
    MGridPos p;
    world_pos -= offset;
    p.x = (int32_t)(world_pos.x/((real_t)region_size_meter));
    p.z = (int32_t)(world_pos.z/((real_t)region_size_meter));
    return p;
}

int8_t MGrid::get_lod_by_distance(const int32_t& dis) {
    for(int8_t i=0 ; i < lod_distance.size(); i++){
        if(dis < lod_distance[i]){
            return i;
        }
    }
    return _chunks->max_lod;
}


void MGrid::set_cam_pos(const Vector3& cam_world_pos) {
    _cam_pos = get_grid_pos(cam_world_pos);
    _cam_pos_real = cam_world_pos;
}



void MGrid::update_search_bound() {
    num_chunks = 0;
    update_mesh_list.clear();
    remove_instance_list.clear();
    MBound sb(_cam_pos, max_range, _size);
    _last_search_bound = _search_bound;
    _search_bound = sb;
}

void MGrid::cull_out_of_bound() {
    RenderingServer* rs = RenderingServer::get_singleton();
    for(int32_t z=_last_search_bound.top; z <=_last_search_bound.bottom; z++)
    {
        for(int32_t x=_last_search_bound.left; x <=_last_search_bound.right; x++){
            if (!_search_bound.has_point(x,z)){
                if(points[z][x].has_instance()){
                    //remove_instance_list.append(points[z][x].instance);
                    remove_instance_list.push_back(points[z][x].instance);
                    points[z][x].instance = RID();
                    points[z][x].mesh = RID();
                }
            }

        }
    }
}


void MGrid::update_lods() {
    /// First Some Clean up
    for(int32_t z=_search_bound.top; z <=_search_bound.bottom; z++){
        for(int32_t x=_search_bound.left; x <=_search_bound.right; x++){
            points[z][x].size = 0;
        }
    }
    //// Now Update LOD
    MGridPos closest = _grid_bound.closest_point_on_ground(_cam_pos);
    closest = get_3d_grid_pos_by_middle_point(closest);
    MBound m(closest);
    int8_t current_lod = 0;
    current_lod = get_lod_by_distance(m.center.get_distance(_cam_pos));
    if(!_grid_bound.has_point(_cam_pos))
    {
        Vector3 closest_real = get_world_pos(closest);
        Vector3 diff = _cam_pos_real - closest_real;
        m.grow_when_outside(diff.x, diff.z,_cam_pos, _search_bound,_chunks->base_size_meter);
        for(int32_t z =m.top; z <= m.bottom; z++){
            for(int32_t x=m.left; x <= m.right; x++){
                points[z][x].lod = current_lod;
                get_region_by_point(x,z)->insert_lod(current_lod);
            }
        }
    } else {
        points[m.center.z][m.center.x].lod = current_lod;
        get_region_by_point(m.center.x,m.center.z)->insert_lod(current_lod);
    }
    while (m.grow(_search_bound,1,1))
    {
        int8_t l;
        if(current_lod != _chunks->max_lod){
            MGridPos e = m.get_edge_point();
            e = get_3d_grid_pos_by_middle_point(e);
            int32_t dis = e.get_distance(_cam_pos);
            l = get_lod_by_distance(dis);
            //make sure that we are going one lod by one lod not jumping two lod
            if (l > current_lod +1){
                l = current_lod + 1;
            }
            // Also make sure to not jump back to lower lod when growing
            if(l<current_lod){
                l = current_lod;
            }
            current_lod = l;
        } else {
            l = _chunks->max_lod;
        }
        if(m.grow_left){
            for(int32_t z=m.top; z<=m.bottom;z++){
                points[z][m.left].lod = l;
                get_region_by_point(m.left,z)->insert_lod(l);
            }
        }
        if(m.grow_right){
            for(int32_t z=m.top; z<=m.bottom;z++){
                points[z][m.right].lod = l;
                get_region_by_point(m.right,z)->insert_lod(l);
            }
        }
        if(m.grow_top){
            for(int32_t x=m.left; x<=m.right; x++){
                points[m.top][x].lod = l;
                get_region_by_point(x,m.top)->insert_lod(l);
            }
        }
        if(m.grow_bottom){
            for(int32_t x=m.left; x<=m.right; x++){
                points[m.bottom][x].lod = l;
                get_region_by_point(x,m.bottom)->insert_lod(l);
            }
        }
    }
}

///////////////////////////////////////////////////////
////////////////// MERGE //////////////////////////////
void MGrid::merge_chunks() {
    for(int i=0; i < _regions_count; i++){
        regions[i].update_region();
    }
    for(int32_t z=_search_bound.top; z<=_search_bound.bottom; z++){
        for(int32_t x=_search_bound.left; x<=_search_bound.right; x++){
            int8_t lod = points[z][x].lod;
            MBound mb(x,z);
            int32_t region_id = get_region_id_by_point(x,z);
            #ifdef NO_MERGE
            check_bigger_size(lod,0, mb);
            num_chunks +=1;
            #else
            for(int8_t s=_chunks->max_size; s>=0; s--){
                if(_chunks->sizes[s].lods[lod].meshes.size()){
                    MBound test_bound = mb;
                    if(test_bound.grow_positive(pow(2,s) - 1, _search_bound)){
                        if(check_bigger_size(lod,s,region_id, test_bound)){
                            num_chunks +=1;
                            break;
                        }
                    }
                }
            }
            #endif
        }
    }
}

//This will check if all lod in this bound are the same if not return false
// also check if the right, bottom, top, left neighbor has the same lod level
// OR on lod level up otherwise return false
// Also if All condition are correct then we can merge to bigger size
// So this will set the size of all points except the first one to -1
// Also Here we should detrmine the edge of each mesh
bool MGrid::check_bigger_size(const int8_t& lod,const int8_t& size,const int32_t& region_id, const MBound& bound) {
    for(int32_t z=bound.top; z<=bound.bottom; z++){
        for(int32_t x=bound.left; x<=bound.right; x++){
            if (points[z][x].lod != lod || points[z][x].size == -1 || get_region_id_by_point(x,z) != region_id)
            {
                return false;
            }
        }
    }
    // So these will determine if left, right, top, bottom edge should adjust to upper LOD
    bool right_edge = false;
    bool left_edge = false;
    bool top_edge = false;
    bool bottom_edge = false;
    // Check right neighbors
    int32_t right_neighbor = bound.right + 1;
    if (right_neighbor <= _search_bound.right){
        // Grab one sample from right neghbor
        int8_t last_right_lod = points[bound.bottom][right_neighbor].lod;
        // Now we don't care what is that all should be same
        for(int32_t z=bound.top; z<bound.bottom; z++){
            if(points[z][right_neighbor].lod != last_right_lod){
                return false;
            }
        }
        right_edge = (last_right_lod == lod + 1);
    }

    // Doing the same for bottom neighbor
    int32_t bottom_neighbor = bound.bottom + 1;
    if(bottom_neighbor <= _search_bound.bottom){
        int8_t last_bottom_lod = points[bottom_neighbor][bound.right].lod;
        for(int32_t x=bound.left; x<bound.right;x++){
            if(points[bottom_neighbor][x].lod != last_bottom_lod){
                return false;
            }
        }
        bottom_edge = (last_bottom_lod == lod + 1);
    }

    // Doing the same for left
    int32_t left_neighbor = bound.left - 1;
    if(left_neighbor >= _search_bound.left){
        int8_t last_left_lod = points[bound.bottom][left_neighbor].lod;
        for(int32_t z=bound.top;z<bound.bottom;z++){
            if(points[z][left_neighbor].lod != last_left_lod){
                return false;
            }
        }
        left_edge = (last_left_lod == lod + 1);
    }
    // WOW finally top neighbor
    int32_t top_neighbor = bound.top - 1;
    if(top_neighbor >= _search_bound.top){
        int8_t last_top_lod = points[top_neighbor][bound.right].lod;
        for(int32_t x=bound.left; x<bound.right; x++){
            if(points[top_neighbor][x].lod != last_top_lod){
                return false;
            }
        }
        top_edge = (last_top_lod == lod + 1);
    }
    // Now all the condition for creating this chunk with this size is true
    // So we start to build that
    // Top left corner will have one chunk with this size
    RenderingServer* rs = RenderingServer::get_singleton();
    for(int32_t z=bound.top; z<=bound.bottom; z++){
        for(int32_t x=bound.left;x<=bound.right;x++){
            if(z==bound.top && x==bound.left){
                points[z][x].size = size;
                int8_t edge_num = get_edge_num(left_edge,right_edge,top_edge,bottom_edge);
                RID mesh = _chunks->sizes[size].lods[lod].meshes[edge_num];
                if(points[z][x].mesh != mesh){
                    points[z][x].mesh = mesh;
                    if(points[z][x].has_instance()){
                        //remove_instance_list.append(points[z][x].instance);
                        remove_instance_list.push_back(points[z][x].instance);
                        points[z][x].instance = RID(); 
                    }
                    MRegion* region = get_region_by_point(x,z);
                    points[z][x].create_instance(get_world_pos(x,0,z), _scenario, region->get_material_rid());
                    rs->instance_set_visible(points[z][x].instance, false);
                    rs->instance_set_base(points[z][x].instance, mesh);
                    //update_mesh_list.append(points[z][x].instance);
                    update_mesh_list.push_back(points[z][x].instance);
                }
            } else {
                points[z][x].size = -1;
                if(points[z][x].has_instance()){
                    //remove_instance_list.append(points[z][x].instance);
                    remove_instance_list.push_back(points[z][x].instance);
                    points[z][x].instance = RID();
                    points[z][x].mesh = RID();
                }
            }
        }
    }
    return true;
}

int8_t MGrid::get_edge_num(const bool& left,const bool& right,const bool& top,const bool& bottom) {
    if(!left && !right && !top && !bottom){
        return M_MAIN;
    }
    if(left && !right && !top && !bottom){
            return M_L;
    }
    if(!left && right && !top && !bottom){
            return M_R;
    }
    if(!left && !right && top && !bottom){
            return M_T;
    }
    if(!left && !right && !top && bottom){
            return M_B;
    }
    if(left && !right && top && !bottom){
            return M_LT;
    }
    if(!left && right && top && !bottom){
            return M_RT;
    }
    if(left && !right && !top && bottom){
            return M_LB;
    }
    if(!left && right && !top && bottom){
            return M_RB;
    }
    if(left && right && top && bottom){
            return M_LRTB;
    }
    UtilityFunctions::print("Error Can not find correct Edge");
    UtilityFunctions::print(left, " ", right, " ", top, " ", bottom);
    return -1;
}

Ref<ShaderMaterial> MGrid::get_material() {
    return _material;
}



void MGrid::set_material(Ref<ShaderMaterial> material) {
    if(material.is_valid()){
        if(material->get_shader().is_valid()){
            _material = material;
        }
    }
}


MGridPos MGrid::get_3d_grid_pos_by_middle_point(MGridPos input) {
    MRegion* r = get_region_by_point(input.x,input.z);
    //Calculating middle point in chunks
    real_t half = ((real_t)_chunks->base_size_meter)/2;
    Vector3 middle_point_chunk = get_world_pos(input) + Vector3(half,0, half);
    middle_point_chunk.y = r->get_closest_height(middle_point_chunk);
    return get_grid_pos(middle_point_chunk);
}

real_t MGrid::get_closest_height(const Vector3& pos) {
    MGridPos grid_pos = get_grid_pos(pos);
    if(!_grid_bound.has_point(grid_pos)){
        return 0;
    }
    MRegion* r = get_region_by_point(grid_pos.x, grid_pos.z);
    return r->get_closest_height(pos);
}

void MGrid::update_chunks(const Vector3& cam_pos) {
    set_cam_pos(cam_pos);
    update_search_bound();
    cull_out_of_bound();
    update_lods();
    merge_chunks();
}

void MGrid::apply_update_chunks() {
    for(int i=0; i < _regions_count; i++){
        regions[i].apply_update();
    }
    RenderingServer* rs = RenderingServer::get_singleton();
    for(RID rm: remove_instance_list){
        rs->free_rid(rm);
    }
    for(RID add : update_mesh_list){
        rs->instance_set_visible(add, true);
    }
}

void MGrid::update_physics(const Vector3& cam_pos){
    MGridPos pos = get_region_pos_by_world_pos(cam_pos);
    MBound bound(pos);
    bound.left -= physics_update_limit;
    bound.right += physics_update_limit;
    bound.top -= physics_update_limit;
    bound.bottom += physics_update_limit;
    //Clear last physics if they are not in the current bound
    for(int32_t z=_last_region_grid_bound.top; z<=_last_region_grid_bound.bottom;z++){
        for(int32_t x=_last_region_grid_bound.left; x<=_last_region_grid_bound.right; x++){
            MGridPos p(x,0,z);
            if(_region_grid_bound.has_point(p) && !bound.has_point(p)){
                get_region(x,z)->remove_physics();
            }
        }
    }
    _last_region_grid_bound = bound;
    for(int32_t z=bound.top; z<=bound.bottom;z++){
        for(int32_t x=bound.left; x<=bound.right; x++){
            MGridPos p(x,0,z);
            if(_region_grid_bound.has_point(p)){
                get_region(x,z)->create_physics();
            }
        }
    }
}

bool MGrid::has_pixel(const uint32_t& x,const uint32_t& y){
    if(x>=pixel_width) return false;
    if(y>=pixel_height) return false;
    if(x<0) return false;
    if(y<0) return false;
    return true;
}

Color MGrid::get_pixel(uint32_t x,uint32_t y, const int32_t& index) {
    if(!has_pixel(x,y)){
        return Color();
    }
    uint32_t ex = (uint32_t)(x%rp == 0 && x!=0);
    uint32_t ey = (uint32_t)(y%rp == 0 && y!=0);
    uint32_t rx = (x/rp) - ex;
    uint32_t ry = (y/rp) - ey;
    x -=rp*rx;
    y -=rp*ry;
    MRegion* r = get_region(rx,ry);
    return r->get_pixel(x,y,index);
}

void MGrid::set_pixel(uint32_t x,uint32_t y,const Color& col,const int32_t& index) {
    if(!has_pixel(x,y)){
        return;
    }
    bool ex = (x%rp == 0 && x!=0);
    bool ey = (y%rp == 0 && y!=0);
    uint32_t rx = (x/rp) - (uint32_t)ex;
    uint32_t ry = (y/rp) - (uint32_t)ey;
    x -=rp*rx;
    y -=rp*ry;
    MRegion* r = get_region(rx,ry);
    r->set_pixel(x,y,col,index);
    // Take care of the edges
    ex = (ex && rx != _region_grid_bound.right);
    // Same for ey
    ey = (ey && ry != _region_grid_bound.bottom);
    if(ex && ey){
        MRegion* re = get_region(rx+1,ry+1);
        re->set_pixel(0,0,col,index);
    }
    if(ex){
        MRegion* re = get_region(rx+1,ry);
        re->set_pixel(0,y,col,index);
    }
    if(ey){
        MRegion* re = get_region(rx,ry+1);
        re->set_pixel(x,0,col,index);
    }
}

real_t MGrid::get_height_by_pixel(uint32_t x,uint32_t y) {
    if(!has_pixel(x,y)){
        return -10000000.0;
    }
    uint32_t ex = (uint32_t)(x%rp == 0 && x!=0);
    uint32_t ey = (uint32_t)(y%rp == 0 && y!=0);
    uint32_t rx = (x/rp) - ex;
    uint32_t ry = (y/rp) - ey;
    x -=rp*rx;
    y -=rp*ry;
    MRegion* r = get_region(rx,ry);
    return r->get_height_by_pixel(x,y);
}

void MGrid::set_height_by_pixel(uint32_t x,uint32_t y,const real_t& value){
    if(!has_pixel(x,y)){
        return;
    }
    bool ex = (x%rp == 0 && x!=0);
    bool ey = (y%rp == 0 && y!=0);
    uint32_t rx = (x/rp) - (uint32_t)ex;
    uint32_t ry = (y/rp) - (uint32_t)ey;
    x -=rp*rx;
    y -=rp*ry;
    MRegion* r = get_region(rx,ry);
    r->set_height_by_pixel(x,y,value);
    // Take care of the edges
    // We dont want ex to be true at the edge of the terrain
    ex = (ex && rx != _region_grid_bound.right);
    // Same for ey
    ey = (ey && ry != _region_grid_bound.bottom);
    if(ex && ey){
        MRegion* re = get_region(rx+1,ry+1);
        re->set_height_by_pixel(0,0,value);
    }
    if(ex){
        MRegion* re = get_region(rx+1,ry);
        re->set_height_by_pixel(0,y,value);
    }
    if(ey){
        MRegion* re = get_region(rx,ry+1);
        re->set_height_by_pixel(x,0,value);
    }
}

void MGrid::generate_normals_thread() {
    Vector<MPixelRegion> px_regions = grid_pixel_region.devide(4);
    Vector<std::thread*> threads_pull;
    UtilityFunctions::print("Generating normals");
    for(int i=0;i<px_regions.size();i++){
        std::thread* t = new std::thread(&MGrid::generate_normals,this, px_regions[i]);
        threads_pull.append(t);
    }
    for(int i=0;i<threads_pull.size();i++){
        std::thread* t = threads_pull[i];
        t->join();
        delete t;
    }
}

void MGrid::generate_normals(MPixelRegion pxr) {
    if(!uniforms_id.has("normals")) return;
    int id = uniforms_id["normals"];
    for(uint32_t y=pxr.top; y<=pxr.bottom; y++){
        for(uint32_t x=pxr.left; x<=pxr.right; x++){
            Vector3 normal_vec;
            Vector2i px(x,y);
            real_t h = get_height_by_pixel(x,y);
            // Caculating face normals around point
            // and average them
            // In total there are 8 face around each point
            for(int i=0;i<nvec8.size()-1;i++){
                Vector2i px1(nvec8[i].x,nvec8[i].z);
                Vector2i px2(nvec8[i+1].x,nvec8[i+1].z);
                px1 += px;
                px2 += px;
                // Edge of the terrain
                if(!has_pixel(px1.x,px1.y) || !has_pixel(px2.x,px2.y)){
                    continue;
                }
                Vector3 vec1 = nvec8[i];
                Vector3 vec2 = nvec8[i+1];
                vec1.y = get_height_by_pixel(px1.x,px1.y) - h;
                vec2.y = get_height_by_pixel(px2.x,px2.y) - h;
                normal_vec += vec1.cross(vec2);
            }
            normal_vec.normalize();
            // packing normals for image
            normal_vec = normal_vec*0.5 + Vector3(0.5,0.5,0.5);
            Color col(normal_vec.x,normal_vec.y,normal_vec.z);
            set_pixel(x,y,col,id);
        }
    }
}

void MGrid::save_image(int index,bool force_save){
    for(int i=0;i<_regions_count;i++){
        regions[i].save_image(index,force_save);
    }
}