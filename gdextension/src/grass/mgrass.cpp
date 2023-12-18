#include "mgrass.h"
#include "../mgrid.h"

#include <godot_cpp/classes/resource_saver.hpp>

#define CHUNK_INFO grid->grid_update_info[grid_index]
#define PS PhysicsServer3D::get_singleton()


void MGrass::_bind_methods() {
    ADD_SIGNAL(MethodInfo("grass_is_ready"));
    ClassDB::bind_method(D_METHOD("set_grass_by_pixel","x","y","val"), &MGrass::set_grass_by_pixel);
    ClassDB::bind_method(D_METHOD("get_grass_by_pixel","x","y"), &MGrass::get_grass_by_pixel);
    ClassDB::bind_method(D_METHOD("update_dirty_chunks"), &MGrass::update_dirty_chunks);
    ClassDB::bind_method(D_METHOD("draw_grass","brush_pos","radius","add"), &MGrass::draw_grass);
    ClassDB::bind_method(D_METHOD("get_count"), &MGrass::get_count);
    ClassDB::bind_method(D_METHOD("get_width"), &MGrass::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &MGrass::get_height);
    ClassDB::bind_method(D_METHOD("grass_px_to_grid_px","x","y"), &MGrass::grass_px_to_grid_px);

    ClassDB::bind_method(D_METHOD("set_active","input"), &MGrass::set_active);
    ClassDB::bind_method(D_METHOD("get_active"), &MGrass::get_active);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,"active"),"set_active","get_active");
    ClassDB::bind_method(D_METHOD("set_grass_data","input"), &MGrass::set_grass_data);
    ClassDB::bind_method(D_METHOD("get_grass_data"), &MGrass::get_grass_data);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"grass_data",PROPERTY_HINT_RESOURCE_TYPE,"MGrassData"),"set_grass_data","get_grass_data");
    ClassDB::bind_method(D_METHOD("set_grass_count_limit","input"), &MGrass::set_grass_count_limit);
    ClassDB::bind_method(D_METHOD("get_grass_count_limit"), &MGrass::get_grass_count_limit);
    ADD_PROPERTY(PropertyInfo(Variant::INT,"grass_count_limit"),"set_grass_count_limit","get_grass_count_limit");
    ClassDB::bind_method(D_METHOD("set_min_grass_cutoff","input"), &MGrass::set_min_grass_cutoff);
    ClassDB::bind_method(D_METHOD("get_min_grass_cutoff"), &MGrass::get_min_grass_cutoff);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "min_grass_cutoff"),"set_min_grass_cutoff","get_min_grass_cutoff");
    ClassDB::bind_method(D_METHOD("set_collision_radius","input"), &MGrass::set_collision_radius);
    ClassDB::bind_method(D_METHOD("get_collision_radius"), &MGrass::get_collision_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"collision_radius"), "set_collision_radius","get_collision_radius");
    ClassDB::bind_method(D_METHOD("set_shape_offset","input"), &MGrass::set_shape_offset);
    ClassDB::bind_method(D_METHOD("get_shape_offset"), &MGrass::get_shape_offset);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3,"shape_offset"), "set_shape_offset","get_shape_offset");
    ClassDB::bind_method(D_METHOD("set_shape","input"), &MGrass::set_shape);
    ClassDB::bind_method(D_METHOD("get_shape"), &MGrass::get_shape);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT,"shape",PROPERTY_HINT_RESOURCE_TYPE,"Shape3D"),"set_shape","get_shape");
    ClassDB::bind_method(D_METHOD("set_active_shape_resize","input"), &MGrass::set_active_shape_resize);
    ClassDB::bind_method(D_METHOD("get_active_shape_resize"), &MGrass::get_active_shape_resize);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL,"active_shape_resize"),"set_active_shape_resize","get_active_shape_resize");
    ClassDB::bind_method(D_METHOD("set_nav_obstacle_radius","input"), &MGrass::set_nav_obstacle_radius);
    ClassDB::bind_method(D_METHOD("get_nav_obstacle_radius"), &MGrass::get_nav_obstacle_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT,"nav_obstacle_radius"),"set_nav_obstacle_radius","get_nav_obstacle_radius");
    ClassDB::bind_method(D_METHOD("set_lod_settings","input"), &MGrass::set_lod_settings);
    ClassDB::bind_method(D_METHOD("get_lod_settings"), &MGrass::get_lod_settings);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,"lod_settings",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_STORAGE), "set_lod_settings","get_lod_settings");
    ClassDB::bind_method(D_METHOD("set_meshes","input"), &MGrass::set_meshes);
    ClassDB::bind_method(D_METHOD("get_meshes"), &MGrass::get_meshes);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,"meshes",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_STORAGE),"set_meshes","get_meshes");
    ClassDB::bind_method(D_METHOD("set_materials"), &MGrass::set_materials);
    ClassDB::bind_method(D_METHOD("get_materials"), &MGrass::get_materials);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY,"materials",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_STORAGE),"set_materials","get_materials");

    ClassDB::bind_method(D_METHOD("save_grass_data"), &MGrass::save_grass_data);

    ClassDB::bind_method(D_METHOD("check_undo"), &MGrass::check_undo);
    ClassDB::bind_method(D_METHOD("undo"), &MGrass::undo);
    ClassDB::bind_method(D_METHOD("_lod_setting_changed"), &MGrass::_lod_setting_changed);
    ClassDB::bind_method(D_METHOD("_grass_tree_exiting"), &MGrass::_grass_tree_exiting);
}

MGrass::MGrass(){
    dirty_points_id = memnew(VSet<int>);
    connect("tree_exiting",Callable(this,"_grass_tree_exiting"));
}
MGrass::~MGrass(){
    memdelete(dirty_points_id);
}

void MGrass::init_grass(MGrid* _grid) {
    ERR_FAIL_COND(!grass_data.is_valid());
    if(!active){
        return;
    }
    grid = _grid;
    scenario = grid->get_scenario();
    space = grid->space;
    region_grid_width = grid->get_region_grid_size().x;
    grass_region_pixel_width = (uint32_t)round((float)grid->region_size_meter/grass_data->density);
    grass_region_pixel_size = grass_region_pixel_width*grass_region_pixel_width;
    base_grid_size_in_pixel = (uint32_t)round((double)grass_region_pixel_width/(double)grid->region_size);
    width = grass_region_pixel_width*grid->get_region_grid_size().x;
    height = grass_region_pixel_width*grid->get_region_grid_size().z;
    grass_pixel_region.left=0;
    grass_pixel_region.top=0;
    grass_pixel_region.right = width - 1;
    grass_pixel_region.bottom = height - 1;
    grass_bound_limit.left = grass_pixel_region.left;
    grass_bound_limit.right = grass_pixel_region.right;
    grass_bound_limit.top = grass_pixel_region.top;
    grass_bound_limit.bottom = grass_pixel_region.bottom;
    int64_t data_size = ((grass_region_pixel_size*grid->get_regions_count() - 1)/8) + 1;
    if(grass_data->data.size()==0){
        // grass data is empty so we create grass data here
        grass_data->data.resize(data_size);
    } else {
        // Data already created so we check if the data size is correct
        ERR_FAIL_COND_EDMSG(grass_data->data.size()!=data_size,"Grass data not match, Some Terrain setting and grass density should not change after grass data creation, change back setting or create a new grass data");
    }
    meshe_rids.clear();
    material_rids.clear();
    for(int i=0;i<meshes.size();i++){
        Ref<Mesh> m = meshes[i];
        if(m.is_valid()){
            meshe_rids.push_back(m->get_rid());
        } else {
            meshe_rids.push_back(RID());
        }
    }
    for(int i=0;i<materials.size();i++){
        Ref<Material> m = materials[i];
        if(m.is_valid()){
            material_rids.push_back(m->get_rid());
        } else {
            material_rids.push_back(RID());
        }
    }
    // Rand num Generation
    default_lod_setting = ResourceLoader::get_singleton()->load("res://addons/m_terrain/default_lod_setting.res");
    for(int i=0;i<lod_settings.size();i++){
        Ref<MGrassLodSetting> s = lod_settings[i];
        if(s.is_valid()){
            settings.push_back(s);
        } else {
            settings.push_back(default_lod_setting);
        }
    }
    for(int i=0;i<settings.size();i++){
        if(!settings[i]->is_connected("lod_setting_changed",Callable(this,"_lod_setting_changed")))
        {
            settings[i]->connect("lod_setting_changed",Callable(this,"_lod_setting_changed"));
        }
        int lod_scale = pow(2,i);
        if(settings[i]->force_lod_count >=0){
            lod_scale = pow(2,settings[i]->force_lod_count);
        }
        float cdensity = grass_data->density*lod_scale;
        rand_buffer_pull.push_back(settings[i]->generate_random_number(cdensity,100));
    }
    // Done
    is_grass_init = true;
    update_grass();
    apply_update_grass();
    //Creating Main Physic Body
    //Setting the shape data
    if(shape.is_valid()){
        main_physics_body = PhysicsServer3D::get_singleton()->body_create();
        PS->body_set_mode(main_physics_body,PhysicsServer3D::BODY_MODE_STATIC);
        PS->body_set_space(main_physics_body,space);
        shape_type = PS->shape_get_type(shape->get_rid());
        shape_data = PS->shape_get_data(shape->get_rid());
    }
    emit_signal("grass_is_ready");
}

void MGrass::clear_grass(){
    std::lock_guard<std::mutex> lock(update_mutex);
    for(HashMap<int64_t,MGrassChunk*>::Iterator it = grid_to_grass.begin();it!=grid_to_grass.end();++it){
        memdelete(it->value);
    }
    for(int i=0;i<rand_buffer_pull.size();i++){
        memdelete(rand_buffer_pull[i]);
    }
    settings.clear();
    rand_buffer_pull.clear();
    grid_to_grass.clear();
    is_grass_init = false;
    final_count = 0;
    if(main_physics_body.is_valid()){
        remove_all_physics();
        PS->body_clear_shapes(main_physics_body);
        PS->free_rid(main_physics_body);
        main_physics_body = RID();
    }
    shapes_ids.clear();
    for(HashMap<int,RID>::Iterator it=shapes_rids.begin();it!=shapes_rids.end();++it){
        PS->free_rid(it->value);
    }
    shapes_rids.clear();
    to_be_visible.clear();
}

void MGrass::update_dirty_chunks(){
    ERR_FAIL_COND(!grass_data.is_valid());
    std::lock_guard<std::mutex> lock(update_mutex);
    for(int i=0;i<dirty_points_id->size();i++){
        int64_t terrain_instance_id = grid->get_point_instance_id_by_point_id((*dirty_points_id)[i]);
        if(!grid_to_grass.has(terrain_instance_id)){
            WARN_PRINT("Dirty point not found "+itos((*dirty_points_id)[i])+ " instance is "+itos(terrain_instance_id));
            continue;
        }
        MGrassChunk* g = grid_to_grass[terrain_instance_id];
        create_grass_chunk(-1,g);
    }
    memdelete(dirty_points_id);
    dirty_points_id = memnew(VSet<int>);
    cull_out_of_bound();
}

void MGrass::update_grass(){
    int new_chunk_count = grid->grid_update_info.size();
    std::lock_guard<std::mutex> lock(update_mutex);
    if(!is_grass_init){
        return;
    }
    update_id = grid->get_update_id();
    for(int i=0;i<new_chunk_count;i++){
        create_grass_chunk(i);
    }
    cull_out_of_bound();
}

void MGrass::cull_out_of_bound(){
    int count_pointer = 0;
    for(int i=0;i<grid->instances_distance.size();i++){
        MGrassChunk* g = grid_to_grass.get(grid->instances_distance[i].id);
        if(count_pointer<grass_count_limit){
            count_pointer += g->total_count;
            if(g->is_part_of_scene){
                g->unrelax();
            }else{
                to_be_visible.push_back(g);
            }
        } else {
            g->relax();
        }
    }
    final_count = count_pointer;
}

void MGrass::create_grass_chunk(int grid_index,MGrassChunk* grass_chunk){
    MGrassChunk* g;
    MPixelRegion px;
    if(grass_chunk==nullptr){
        px.left = (uint32_t)round(((double)grass_region_pixel_width)*CHUNK_INFO.region_offset_ratio.x);
        px.top = (uint32_t)round(((double)grass_region_pixel_width)*CHUNK_INFO.region_offset_ratio.y);
        int size_scale = pow(2,CHUNK_INFO.chunk_size);
        px.right = px.left + base_grid_size_in_pixel*size_scale - 1;
        px.bottom = px.top + base_grid_size_in_pixel*size_scale - 1;
        // We keep the chunk information for grass only in root grass chunk
        g = memnew(MGrassChunk(px,CHUNK_INFO.region_world_pos,CHUNK_INFO.lod,CHUNK_INFO.region_id));
        grid_to_grass.insert(CHUNK_INFO.terrain_instance.get_id(),g);
    } else {
        g = grass_chunk;
        // We clear tree to create everything again from start
        g->clear_tree();
        px = grass_chunk->pixel_region;
    }
    int lod_scale;
    int rand_buffer_size = rand_buffer_pull[g->lod]->size()/12;
    const float* rand_buffer =(float*)rand_buffer_pull[g->lod]->ptr();
    if(settings[g->lod]->force_lod_count >=0 && settings[g->lod]->force_lod_count < lod_count){
        lod_scale = pow(2,settings[g->lod]->force_lod_count);
    } else {
        lod_scale = pow(2,g->lod);
    }
    int grass_region_pixel_width_lod = grass_region_pixel_width/lod_scale;

    uint32_t divide_amount= (uint32_t)settings[g->lod]->divide;
    Vector<MPixelRegion> pixel_regions = px.devide(divide_amount);
    int grass_in_cell = settings[g->lod]->grass_in_cell;

    

    const uint8_t* ptr = grass_data->data.ptr() + g->region_id*grass_region_pixel_size/8;

    MGrassChunk* root_g=g;
    MGrassChunk* last_g=g;
    uint32_t total_count=0;
    for(int k=0;k<pixel_regions.size();k++){
        px = pixel_regions[k];
        if(k!=0){
            g = memnew(MGrassChunk());
            last_g->next = g;
            last_g = g;
        }
        uint32_t count=0;
        uint32_t index;
        uint32_t x=px.left;
        uint32_t y=px.top;
        uint32_t i=0;
        uint32_t j=1;
        PackedFloat32Array buffer;
        while (true)
        {
            while (true){
                x = px.left + i*lod_scale;
                if(x>px.right){
                    break;
                }
                i++;
                uint32_t offs = (y*grass_region_pixel_width + x);
                uint32_t ibyte = offs/8;
                uint32_t ibit = offs%8;
                if( (ptr[ibyte] & (1 << ibit)) != 0){
                    for(int r=0;r<grass_in_cell;r++){
                        index=count*BUFFER_STRID_FLOAT;
                        int rand_index = y*grass_region_pixel_width_lod + x*grass_in_cell + r;
                        const float* ptr = rand_buffer + (rand_index%rand_buffer_size)*BUFFER_STRID_FLOAT;
                        buffer.resize(buffer.size()+12);
                        float* ptrw = (float*)buffer.ptrw();
                        ptrw += index;
                        mempcpy(ptrw,ptr,BUFFER_STRID_BYTE);
                        Vector3 pos;
                        pos.x = root_g->world_pos.x + x*grass_data->density + ptrw[3];
                        pos.z = root_g->world_pos.z + y*grass_data->density + ptrw[11];
                        ptrw[7] += grid->get_height(pos);
                        if(std::isnan(ptrw[7])){
                            buffer.resize(buffer.size()-12);
                            continue;
                        }
                        ptrw[3] = pos.x;
                        ptrw[11] = pos.z;
                        count++;
                    }
                }
            }
            i= 0;
            y= px.top + j*lod_scale;
            if(y>px.bottom){
                break;
            }
            j++;
        }
        // Discard grass chunk in case there is no mesh RID or count is less than min_grass_cutoff
        if(meshe_rids[root_g->lod] == RID() || count < min_grass_cutoff){
            g->set_buffer(0,RID(),RID(),RID(),PackedFloat32Array());
            continue;
        }
        g->set_buffer(count,scenario,meshe_rids[root_g->lod],material_rids[root_g->lod],buffer);
        total_count += count;
    }
    root_g->total_count = total_count;
    if(grass_chunk!=nullptr){ // This means updating not creating
            root_g->unrelax();
    }
}



void MGrass::apply_update_grass(){
    if(!is_grass_init){
        return;
    }
    for(int i=0;i<to_be_visible.size();i++){
        if(!to_be_visible[i]->is_out_of_range){
            to_be_visible[i]->unrelax();
            to_be_visible[i]->is_part_of_scene = true;
        }
    }
    for(int i=0;i<grid->remove_instance_list.size();i++){
        if(grid_to_grass.has(grid->remove_instance_list[i].get_id())){
            MGrassChunk* g = grid_to_grass.get(grid->remove_instance_list[i].get_id());
            memdelete(g);
            grid_to_grass.erase(grid->remove_instance_list[i].get_id());
        } else {
            WARN_PRINT("Instance not found for removing");
        }
    }
    to_be_visible.clear();
}

void MGrass::recalculate_grass_config(int max_lod){
    lod_count = max_lod + 1;
    if(meshes.size()!=lod_count){
        meshes.resize(lod_count);
    }
    if(materials.size()!=lod_count){
        materials.resize(lod_count);
    }
    if(lod_settings.size()!=lod_count){
        lod_settings.resize(lod_count);
    }
    notify_property_list_changed();
}

void MGrass::set_grass_by_pixel(uint32_t px, uint32_t py, bool p_value){
    ERR_FAIL_INDEX(px, width);
    ERR_FAIL_INDEX(py, height);
    Vector2 flat_pos(float(px)*grass_data->density,float(py)*grass_data->density);
    int point_id = grid->get_point_id_by_non_offs_ws(flat_pos);
    dirty_points_id->insert(point_id);
    uint32_t rx = px/grass_region_pixel_width;
    uint32_t ry = py/grass_region_pixel_width;
    uint32_t rid = ry*region_grid_width + rx;
    uint32_t x = px%grass_region_pixel_width;
    uint32_t y = py%grass_region_pixel_width;
    uint32_t offs = rid*grass_region_pixel_size + y*grass_region_pixel_width + x;
    uint32_t ibyte = offs/8;
    uint32_t ibit = offs%8;

    uint8_t b = grass_data->data[ibyte];

    if(p_value){
        b |= (1 << ibit);
    } else {
        b &= ~(1 << ibit);
    }
    grass_data->data.set(ibyte,b);
}

bool MGrass::get_grass_by_pixel(uint32_t px, uint32_t py) {
    ERR_FAIL_INDEX_V(px, width,false);
    ERR_FAIL_INDEX_V(py, height,false);
    uint32_t rx = px/grass_region_pixel_width;
    uint32_t ry = py/grass_region_pixel_width;
    uint32_t rid = ry*region_grid_width + rx;
    uint32_t x = px%grass_region_pixel_width;
    uint32_t y = py%grass_region_pixel_width;
    uint32_t offs = rid*grass_region_pixel_size + y*grass_region_pixel_width + x;
    uint32_t ibyte = offs/8;
    uint32_t ibit = offs%8;
    return (grass_data->data[ibyte] & (1 << ibit)) != 0;
}

Vector2i MGrass::get_closest_pixel(Vector3 pos){
    pos -= grid->offset;
    pos = pos / grass_data->density;
    return Vector2i(round(pos.x),round(pos.z));
}

Vector3 MGrass::get_pixel_world_pos(uint32_t px, uint32_t py){
    Vector3 out(0,0,0);
    ERR_FAIL_COND_V(!grass_data.is_valid(),out);
    out.x = grid->offset.x + ((float)px)*grass_data->density;
    out.z = grid->offset.z + ((float)py)*grass_data->density;
    return out;
}

Vector2i MGrass::grass_px_to_grid_px(uint32_t px, uint32_t py){
    Vector2 v;
    ERR_FAIL_COND_V(!grass_data.is_valid(),Vector2i(v));
    v = Vector2(px,py)*grass_data->density;
    v = v/grid->get_h_scale();
    return Vector2i(round(v.x),round(v.y));
}

// At least for now it is not safe to put this function inside a thread
// because set_grass_by_pixel is chaning dirty_points_id
// And I don't think that we need to do that because it is not a huge process
void MGrass::draw_grass(Vector3 brush_pos,real_t radius,bool add){
    ERR_FAIL_COND(!is_grass_init);
    ERR_FAIL_COND(update_id!=grid->get_update_id());
    Vector2i px_pos = get_closest_pixel(brush_pos);
    if(px_pos.x<0 || px_pos.y<0 || px_pos.x>width || px_pos.y>height){
        return;
    }
    uint32_t brush_px_radius = (uint32_t)(radius/grass_data->density);
    uint32_t brush_px_pos_x = px_pos.x;
    uint32_t brush_px_pos_y = px_pos.y;
    // Setting left right top bottom
    MPixelRegion px;
    px.left = (brush_px_pos_x>brush_px_radius) ? brush_px_pos_x - brush_px_radius : 0;
    px.right = brush_px_pos_x + brush_px_radius;
    px.right = px.right > grass_pixel_region.right ? grass_pixel_region.right : px.right;
    px.top = (brush_px_pos_y>brush_px_radius) ? brush_px_pos_y - brush_px_radius : 0;
    px.bottom = brush_px_pos_y + brush_px_radius;
    px.bottom = (px.bottom>grass_pixel_region.bottom) ? grass_pixel_region.bottom : px.bottom;
    //UtilityFunctions::print("brush pos ", brush_pos);
    //UtilityFunctions::print("draw R ",brush_px_radius);
    //UtilityFunctions::print("L ",itos(px.left)," R ",itos(px.right)," T ",itos(px.top), " B ",itos(px.bottom));
    // LOD Scale
    //int lod_scale = pow(2,lod);
    // LOOP
    uint32_t x=px.left;
    uint32_t y=px.top;
    uint32_t i=0;
    uint32_t j=1;
    for(uint32_t y = px.top; y<=px.bottom;y++){
        for(uint32_t x = px.left; x<=px.right;x++){
            uint32_t dif_x = abs(x - brush_px_pos_x);
            uint32_t dif_y = abs(y - brush_px_pos_y);
            uint32_t dis = sqrt(dif_x*dif_x + dif_y*dif_y);
            Vector2i grid_px = grass_px_to_grid_px(x,y);
            if(dis<brush_px_radius && grid->get_brush_mask_value_bool(grid_px.x,grid_px.y))
                set_grass_by_pixel(x,y,add);
        }
    }
    update_dirty_chunks();
}
void MGrass::set_active(bool input){
    active = input;
}
bool MGrass::get_active(){
    return active;
}
void MGrass::set_grass_data(Ref<MGrassData> d){
    grass_data = d;
}

Ref<MGrassData> MGrass::get_grass_data(){
    return grass_data;
}

void MGrass::set_grass_count_limit(int input){
    grass_count_limit = input;
}
int MGrass::get_grass_count_limit(){
    return grass_count_limit;
}

void MGrass::set_min_grass_cutoff(int input){
    ERR_FAIL_COND(input<0);
    min_grass_cutoff = input;
}

int MGrass::get_min_grass_cutoff(){
    return min_grass_cutoff;
}

void MGrass::set_lod_settings(Array input){
    lod_settings = input;
}
Array MGrass::get_lod_settings(){
    return lod_settings;
}

void MGrass::set_meshes(Array input){
    meshes = input;
}
Array MGrass::get_meshes(){
    return meshes;
}

void MGrass::set_materials(Array input){
    materials = input;
}

Array MGrass::get_materials(){
    return materials;
}

uint32_t MGrass::get_width(){
    return width;
}
uint32_t MGrass::get_height(){
    return height;
}

int64_t MGrass::get_count(){
    return final_count;
}

void MGrass::set_collision_radius(float input){
    collision_radius=input;
}

float MGrass::get_collision_radius(){
    return collision_radius;
}

void MGrass::set_shape_offset(Vector3 input){
    shape_offset = input;
}

Vector3 MGrass::get_shape_offset(){
    return shape_offset;
}

void MGrass::set_shape(Ref<Shape3D> input){
    shape = input;
}
Ref<Shape3D> MGrass::get_shape(){
    return shape;
}

void MGrass::set_active_shape_resize(bool input){
    active_shape_resize = input;
}

bool MGrass::get_active_shape_resize(){
    return active_shape_resize;
}

void MGrass::set_nav_obstacle_radius(float input){
    ERR_FAIL_COND(input<0.05);
    nav_obstacle_radius = input;
}
float MGrass::get_nav_obstacle_radius(){
    return nav_obstacle_radius;
}
/*
Vector3 pos;
pos.x = root_g->world_pos.x + x*grass_data->density + ptrw[3];
pos.z = root_g->world_pos.z + y*grass_data->density + ptrw[11];
ptrw[3] = pos.x;
ptrw[7] += grid->get_height(pos);
ptrw[11] = pos.z;
uint32_t rx = x/grass_region_pixel_width;
uint32_t ry = y/grass_region_pixel_width;
*/
void MGrass::update_physics(Vector3 cam_pos){
    ERR_FAIL_COND(!main_physics_body.is_valid());
    if(!shape.is_valid()){
        return;
    }
    ERR_FAIL_COND(!is_grass_init);
    int grass_in_cell = settings[0]->grass_in_cell;
    cam_pos -= grid->offset;
    cam_pos = cam_pos / grass_data->density;
    int px_x = round(cam_pos.x);
    int px_y = round(cam_pos.z);
    int col_r = round(collision_radius/grass_data->density);
    if(px_x < - col_r || px_y < - col_r){
        remove_all_physics();
        return;
    }
    physics_search_bound = MBound(MGridPos(px_x,0,px_y));
    physics_search_bound.grow(grass_bound_limit,col_r,col_r);
    if(!(physics_search_bound.left <width && physics_search_bound.top<height)){
        remove_all_physics();
        return;
    }
    //UtilityFunctions::print("Left ",physics_search_bound.left," right ",physics_search_bound.right," top ",physics_search_bound.top," bottom ",physics_search_bound.bottom );
    // culling
    /// Removing out of bound shapes
    int remove_count=0;
    for(int y=last_physics_search_bound.top;y<=last_physics_search_bound.bottom;y++){
        for(int x=last_physics_search_bound.left;x<=last_physics_search_bound.right;x++){
            if(!physics_search_bound.has_point(x,y)){
                for(int r=0;r<grass_in_cell;r++){
                    uint64_t uid = y*width*grass_in_cell + x*grass_in_cell + r;
                    int find_index = shapes_ids.find(uid);
                    if(find_index!=-1){
                        PS->body_remove_shape(main_physics_body,find_index);
                        shapes_ids.remove_at(find_index);
                    }
                }
            }
        }
    }
    last_physics_search_bound = physics_search_bound;
    const float* rand_buffer =(float*)rand_buffer_pull[0]->ptr();
    int rand_buffer_size = rand_buffer_pull[0]->size()/12;
    int update_count = 0;
    for(uint32_t y=physics_search_bound.top;y<=physics_search_bound.bottom;y++){
        for(uint32_t x=physics_search_bound.left;x<=physics_search_bound.right;x++){
            if(!get_grass_by_pixel(x,y)){
                continue;
            }
            for(int r=0;r<grass_in_cell;r++){
                uint64_t uid = y*width*grass_in_cell + x*grass_in_cell + r;
                if(shapes_ids.has(uid)){
                    continue;
                }
                int rx = (x/grass_region_pixel_width);
                int ry = (y/grass_region_pixel_width);
                int rand_index = (y-ry*grass_region_pixel_width)*grass_region_pixel_width + (x-rx*grass_region_pixel_width)*grass_in_cell + r;
                //UtilityFunctions::print("grass_region_pixel_width ", grass_region_pixel_width);
                //UtilityFunctions::print("X ",x, " Y ", y, " RX ",rx, " RY ", ry);
                //UtilityFunctions::print("rand_index ",(rand_index));
                const float* ptr = rand_buffer + (rand_index%rand_buffer_size)*BUFFER_STRID_FLOAT;
                Vector3 wpos(x*grass_data->density+ptr[3],0,y*grass_data->density+ptr[11]);
                //UtilityFunctions::print("Physic pos ",wpos);
                wpos += grid->offset;
                wpos.y = grid->get_height(wpos) + ptr[7];
                if(std::isnan(wpos.y)){
                    continue;
                }
                // Godot physics not work properly with collission transformation
                // So for now we ignore transformation
                Vector3 x_axis(ptr[0],ptr[4],ptr[8]);
                Vector3 y_axis(ptr[1],ptr[5],ptr[9]);
                Vector3 z_axis(ptr[2],ptr[6],ptr[10]);
                ///
                //Vector3 x_axis(1,0,0);
                //Vector3 y_axis(0,1,0);
                //Vector3 z_axis(0,0,1);
                Basis b_no_normlized(x_axis,y_axis,z_axis);
                x_axis.normalize();
                y_axis.normalize();
                z_axis.normalize();
                Basis b(x_axis,y_axis,z_axis);
                if((shape_type==PhysicsServer3D::ShapeType::SHAPE_CONVEX_POLYGON || shape_type==PhysicsServer3D::ShapeType::SHAPE_CONCAVE_POLYGON)){
                    //As tested godot physics is ok with scaling
                    //So we make some exception here
                    Transform3D final_t(Basis(),shape_offset);
                    final_t = Transform3D(b_no_normlized,wpos)*final_t;
                    PS->body_add_shape(main_physics_body,shape->get_rid(),final_t);
                }
                else if(active_shape_resize){
                    RID s_rid;
                    Vector3 scale = b_no_normlized.get_scale();
                    Vector3 offset = shape_offset*scale; //If this has offset we should currect the offset
                    Transform3D final_t(Basis(),offset);
                    Transform3D t(b,wpos);
                    final_t = t * final_t;
                    if(shapes_rids.has(rand_index)){
                        s_rid = shapes_rids[rand_index];
                    } else {
                        s_rid = get_resized_shape(scale);
                        shapes_rids.insert(rand_index,s_rid);
                    }
                    PS->body_add_shape(main_physics_body,s_rid,final_t);
                } else{
                    Vector3 offset = shape_offset;
                    Transform3D final_t(Basis(),offset);
                    Transform3D t(b,wpos);
                    final_t = t * final_t;
                    PS->body_add_shape(main_physics_body,shape->get_rid(),final_t);
                }
                shapes_ids.push_back(uid);
                update_count++;
            }
        }
    }
}

void MGrass::remove_all_physics(){
    if(shape.is_valid()){
        PS->body_clear_shapes(main_physics_body);
        shapes_ids.clear();
    }
}

RID MGrass::get_resized_shape(Vector3 scale){
    RID new_shape;
    ERR_FAIL_COND_V(!shape.is_valid(),new_shape);
    if(shape_type==PhysicsServer3D::ShapeType::SHAPE_SPHERE){
        float max_s = scale.x > scale.y ? scale.x : scale.y;
        max_s = max_s > scale.z ? max_s : scale.z;
        float r = shape_data;
        r = r*max_s;
        new_shape = PS->sphere_shape_create();
        PS->shape_set_data(new_shape,r);
    } else if(shape_type==PhysicsServer3D::ShapeType::SHAPE_BOX){
        Vector3 d = shape_data;
        d = d*scale;
        new_shape = PS->box_shape_create();
        PS->shape_set_data(new_shape,d);
    } else if(shape_type==PhysicsServer3D::ShapeType::SHAPE_CYLINDER){
        Dictionary d = shape_data;
        float max_xz = scale.x > scale.z ? scale.x : scale.z;
        float r = d["radius"];
        r = r*max_xz;
        float h = d["height"];
        h = h*scale.y;
        Dictionary new_d;
        new_d["radius"] = r;
        new_d["height"] = h;
        new_shape = PS->cylinder_shape_create();
        PS->shape_set_data(new_shape,new_d);
    } else if(shape_type==PhysicsServer3D::ShapeType::SHAPE_CAPSULE){
        Dictionary d = shape_data;
        float max_xz = scale.x > scale.z ? scale.x : scale.z;
        float r = d["radius"];
        r = r*max_xz;
        float h = d["height"];
        h = h*scale.y;
        Dictionary new_d;
        new_d["radius"] = r;
        new_d["height"] = h;
        new_shape = PS->capsule_shape_create();
        PS->shape_set_data(new_shape,new_d);
    } else{
        WARN_PRINT("Shape Type "+itos(shape_type)+" Does not support resizing");
        new_shape = shape->get_rid();
    }
    return new_shape;
}

PackedVector3Array MGrass::get_physic_positions(Vector3 cam_pos,float radius){
    PackedVector3Array positions;
   if(!shape.is_valid()){
        return positions;
    }
    ERR_FAIL_COND_V(!is_grass_init,positions);
    int grass_in_cell = settings[0]->grass_in_cell;
    cam_pos -= grid->offset;
    cam_pos = cam_pos / grass_data->density;
    int px_x = round(cam_pos.x);
    int px_y = round(cam_pos.z);
    int col_r = round(radius/grass_data->density);
    if(px_x < -col_r || px_y < -col_r){
        return positions;
    }
    MPixelRegion bound;
    bound.left = px_x > col_r ? px_x - col_r : 0;
    bound.top = px_y > col_r ? px_y - col_r : 0;
    if(!(bound.left <width && bound.top<height)){
        return positions;
    }
    bound.right = px_x + col_r;
    bound.right = bound.right < width ? bound.right : width - 1;
    bound.bottom = px_y + col_r;
    bound.bottom = bound.bottom < height ? bound.bottom : height - 1;
    const float* rand_buffer =(float*)rand_buffer_pull[0]->ptr();
    int rand_buffer_size = rand_buffer_pull[0]->size()/12;
    for(uint32_t y=bound.top;y<=bound.bottom;y++){
        for(uint32_t x=bound.left;x<=bound.right;x++){
            if(!get_grass_by_pixel(x,y)){
                continue;
            }
            for(int r=0;r<grass_in_cell;r++){
                int rx = (x/grass_region_pixel_width);
                int ry = (y/grass_region_pixel_width);
                int rand_index = (y-ry*grass_region_pixel_width)*grass_region_pixel_width + (x-rx*grass_region_pixel_width) + r;
                const float* ptr = rand_buffer + (rand_index%rand_buffer_size)*BUFFER_STRID_FLOAT;
                Vector3 wpos(x*grass_data->density+ptr[3],0,y*grass_data->density+ptr[11]);
                wpos += grid->offset;
                wpos.y = grid->get_height(wpos) + ptr[7];
                wpos += shape_offset;
                positions.push_back(wpos);
            }
        }
    }
    return positions;
}

void MGrass::_get_property_list(List<PropertyInfo> *p_list) const{
    PropertyInfo sub_lod0(Variant::INT, "LOD Settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_SUBGROUP);
    p_list->push_back(sub_lod0);
    for(int i=0;i<materials.size();i++){
        PropertyInfo m(Variant::OBJECT,"Setting_LOD_"+itos(i),PROPERTY_HINT_RESOURCE_TYPE,"MGrassLodSetting",PROPERTY_USAGE_EDITOR);
        p_list->push_back(m);
    }
    PropertyInfo sub_lod(Variant::INT, "Grass Materials", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_SUBGROUP);
    p_list->push_back(sub_lod);
    for(int i=0;i<materials.size();i++){
        PropertyInfo m(Variant::OBJECT,"Material_LOD_"+itos(i),PROPERTY_HINT_RESOURCE_TYPE,"StandardMaterial3D,ORMMaterial3D,ShaderMaterial",PROPERTY_USAGE_EDITOR);
        p_list->push_back(m);
    }
    PropertyInfo sub_lod2(Variant::INT, "Grass Meshes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_SUBGROUP);
    p_list->push_back(sub_lod2);
    for(int i=0;i<meshes.size();i++){
        PropertyInfo m(Variant::OBJECT,"Mesh_LOD_"+itos(i),PROPERTY_HINT_RESOURCE_TYPE,"Mesh",PROPERTY_USAGE_EDITOR);
        p_list->push_back(m);
    }
}

bool MGrass::_get(const StringName &p_name, Variant &r_ret) const{
    if(p_name.begins_with("Material_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>materials.size()-1){
            return false;
        }
        r_ret = materials[index];
        return true;
    }
    if(p_name.begins_with("Mesh_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>meshes.size()-1){
            return false;
        }
        r_ret = meshes[index];
        return true;
    }
    if(p_name.begins_with("Setting_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>lod_settings.size()-1){
            return false;
        }
        r_ret = lod_settings[index];
        return true;
    }
    return false;
}
bool MGrass::_set(const StringName &p_name, const Variant &p_value){
    if(p_name.begins_with("Material_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>materials.size()-1){
            return false;
        }
        materials[index] = p_value;
        if(is_grass_init){
            Ref<Material> m = p_value;
            if(m.is_valid()){
                material_rids.set(index,m->get_rid());
            } else {
                material_rids.set(index,RID());
            }
            recreate_all_grass(index);
        }
        return true;
    }
    if(p_name.begins_with("Mesh_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>meshes.size()-1){
            return false;
        }
        meshes[index] = p_value;
        if(is_grass_init){
            Ref<Mesh> m = p_value;
            if(m.is_valid()){
                meshe_rids.set(index,m->get_rid());
            } else {
                meshe_rids.set(index,RID());
            }
            recreate_all_grass(index);
        }
        return true;
    }
    if(p_name.begins_with("Setting_LOD_")){
        PackedStringArray s = p_name.split("_");
        int index = s[2].to_int();
        if(index>lod_settings.size()-1){
            return false;
        }
        lod_settings[index] = p_value;
        Ref<MGrassLodSetting> setting = p_value;
        if(is_grass_init){
            if(!setting.is_valid()){
                setting = ResourceLoader::get_singleton()->load("res://addons/m_terrain/default_lod_setting.res");
            }
            settings.set(index,setting);
            settings[index]->is_dirty = true;
            _lod_setting_changed();
        }
        return true;
    }
    return false;
}

godot::Error MGrass::save_grass_data(){
    if(grass_data.is_valid()){
        return ResourceSaver::get_singleton()->save(grass_data,grass_data->get_path());
    }
    return ERR_UNAVAILABLE;   
}

void MGrass::recreate_all_grass(int lod){
    for(HashMap<int64_t,MGrassChunk*>::Iterator it = grid_to_grass.begin();it!=grid_to_grass.end();++it){
        if(lod==-1){
            create_grass_chunk(-1,it->value);
        } else if (lod==it->value->lod)
        {
            create_grass_chunk(-1,it->value);
        }
    }
}

void MGrass::update_random_buffer_pull(int lod){
    ERR_FAIL_INDEX(lod,rand_buffer_pull.size());
    memdelete(rand_buffer_pull[lod]);
    int lod_scale;
    if(settings[lod]->force_lod_count >=0){
        lod_scale = pow(2,settings[lod]->force_lod_count);
    } else {
        lod_scale = pow(2,lod);
    }
    float cdensity = grass_data->density*lod_scale;
    rand_buffer_pull.set(lod,settings[lod]->generate_random_number(cdensity,100));
}

void MGrass::_lod_setting_changed(){
    for(int i=0;i<lod_count;i++){
        if(settings[i].is_valid()){
            if(settings[i]->is_dirty){
                update_random_buffer_pull(i);
                recreate_all_grass(i);
            }
        }
    }
    for(int i=0;i<lod_count;i++){
        if(settings[i].is_valid()){
            settings[i]->is_dirty = false;
        }
    }
}

void MGrass::check_undo(){
    ERR_FAIL_COND(!grass_data.is_valid());
    grass_data->check_undo();
}

void MGrass::undo(){
    ERR_FAIL_COND(!grass_data.is_valid());
    grass_data->undo();
    recreate_all_grass();
}

void MGrass::_grass_tree_exiting(){
    clear_grass();
}