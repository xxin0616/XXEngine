#include "rasterizer.hpp"
#include "Triangle.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "OBJ_Loader.h"
#include "Bezier.hpp"
#include <Eigen/Eigen>
#include <math.h>
#include <opencv2/opencv.hpp>

// http://games-cn.org/forums/topic/allhw/
// 浣滀笟1锛氬疄鐜?model銆乸rojection 鐭╅樀锛涚粫浠绘剰杞存棆杞?
// 浣滀笟2锛氬疄鐜?涓夎褰㈠～鍏呫€佹繁搴︾紦鍐诧紙Z-Buffer锛夈€丮SAA锛堝閲嶉噰鏍锋姉閿娇锛?
// 浣滀笟3锛氬疄鐜?Blinn-Phong 妯″瀷 + Bump Mapping
//       Blinn-Phong 妯″瀷鏄珮鍏夊弽灏勬ā鍨嬶紝铏界劧宸茬粡琚玃BR鍙栦唬锛屼絾渚濈劧鍦ㄩ鏍煎寲娓叉煋銆佺Щ鍔ㄧ涓娇鐢?
// 浣滀笟4锛氬疄鐜拌礉濉炲皵鏇茬嚎锛屾洸绾挎姉閿娇
// 浣滀笟5锛氬厜绾夸笌涓夎褰㈢浉浜?
// 浣滀笟6锛氬姞閫熺粨鏋?BVH
// 浣滀笟7锛氳矾寰勮拷韪€佸井琛ㄩ潰妯″瀷
// 浣滀笟8锛氳川鐐瑰脊绨х郴缁?


using namespace Eigen;

int width = 700;
int height = 700;
Vector3f eye_pos{ 0,1,5 };

Matrix4f get_view_matrix(Vector3f eye_pos) {
	Matrix4f view = Matrix4f::Identity();
	Matrix4f translate;
	translate << 1, 0, 0, -eye_pos[0],
				 0, 1, 0, -eye_pos[1],
				 0, 0, 1, -eye_pos[2],
				 0, 0, 0, 1;
	view = translate * view;
	return view;
}

Matrix4f get_model_matrix(float angle, float scaleSize = 1) {
	Eigen::Matrix4f rotation;
	angle = angle * MY_PI / 180.f;
	rotation << cos(angle), 0, sin(angle), 0,
		0, 1, 0, 0,
		-sin(angle), 0, cos(angle), 0,
		0, 0, 0, 1;
	//缁晊杞存棆杞?

	Eigen::Matrix4f scale;
	scale << scaleSize, 0, 0, 0,
		0, scaleSize, 0, 0,
		0, 0, scaleSize, 0,
		0, 0, 0, 1;
	//鏀惧ぇ2.5鍊?

	Eigen::Matrix4f translate;
	translate << 1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1;
	//涓嶅钩绉?

	return translate * rotation * scale;
}

//Matrix4f get_axis_model_matrix(float rotation_angle, Vector3f axis) {
//	//TODO 缁曚换鎰忚酱鏃嬭浆
//}

Matrix4f get_projection_matrix(float eye_fov, float zNear, float zFar) {
	float aspect_ratio = width / height;
	Eigen::Matrix4f perspective;
	float half_fov = eye_fov / 180 * MY_PI / 2;
	float cot_h_f = 1 / tan(half_fov);
	perspective << cot_h_f / aspect_ratio, 0, 0, 0,
		0, cot_h_f, 0, 0,
		0, 0, (zNear + zFar) / (zFar - zNear), - 2 * zNear * zFar / (zFar - zNear),
		0, 0, -1, 0;

	return perspective;
}

Matrix4f get_orthographic_projection_matrix(float eye_fov, float zNear, float zFar) {
	Matrix4f projection = Matrix4f::Identity();
	float aspect_ratio = width / height;

	float angle = (eye_fov / 2) * MY_PI / 180;

	float top = zNear * tan(angle);
	float bottom = -top;
	float right = top * aspect_ratio;
	float left = -right;

	projection << 2.0f / (right - left), 0, 0, -(right + left) / (right - left),
		0, 2.0f / (top - bottom), 0, -(top + bottom) / (top - bottom),
		0, 0, 2.0f / (zNear - zFar), -(zFar + zNear) / (zNear - zFar),
		0, 0, 0, 1;

	return projection;
}

Vector3f vertex_shader(const vertex_shader_payload& payload) {
	return payload.position;
}

Vector3f normal_fragment_shader(const fragment_shader_payload& payload) {
	Vector3f return_color = (payload.normal.head<3>().normalized() + Vector3f(1.0f, 1.0f, 1.0f)) / 2.f;
	/*Vector3f result;
	result << return_color.x() , return_color.y() * 255, return_color.z() * 255;*/
	return return_color;
}

Vector3f basic_fragment_shader(const fragment_shader_payload& payload) {
	return payload.color;
}

Vector3f basic_texture_fragment_shader(const fragment_shader_payload& payload) {
	return payload.texture->getColorBilinear(payload.tex_coords);
}

Vector3f test_fragment_shader(const fragment_shader_payload& payload) {
	return Vector3f(payload.tex_coords[0], payload.tex_coords[1], 0);
}

Vector3f phong_fragment_shader(const fragment_shader_payload& payload) {
	Vector3f ka = Vector3f(0.005, 0.005, 0.005);
	Vector3f kd = payload.color;
	Vector3f ks = Vector3f(0.7937, 0.7937, 0.7937);

	auto l1 = light{ {20,20,20},{500,500,500} };
	auto l2 = light{ {-20,20,0},{500,500,500} };

	std::vector<light> lights = { l1,l2 };
	Vector3f amb_light_intensity{ 10,10,10 };
	// 杩欎釜 eye_pos 鐨勪綅缃簲璇ユ槸鍏ㄥ眬鍙橀噺

	float shiness = 150;

	Vector3f color = payload.color;
	Vector3f point = payload.view_pos;
	Vector3f normal = payload.normal;

	Vector3f result_color = { 0,0,0 };
	for (auto& light : lights) {
		Vector3f light_dir = (light.position - point).normalized();
		Vector3f view_dir = (eye_pos - point).normalized();
		Vector3f half_vector = (light_dir + view_dir).normalized();
		normal.normalize();

		float distance_square = (light.position - point).dot(light.position - point);
		// 璺濈骞虫柟
		Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
		// 閫愬厓绱犱箻娉?
		Vector3f diffuse = kd.cwiseProduct(light.intensity / distance_square) * 
			std::max(0.0f, normal.dot(light_dir));
		// 鐐圭Н鐨勭粨鏋滄槸涓€涓爣閲忥紙閮芥槸鍗曚綅鍚戦噺鐨勬椂鍊欙紝cos theata锛夛紝鍊煎湪锛?1锛?锛夛紝鍚屽悜鏈€澶т负1锛屽瀭鐩翠负0锛屽弽鍚?1
		Vector3f specular = ks.cwiseProduct(light.intensity / distance_square) *
			std::pow(std::max(0.0f, normal.dot(half_vector)), shiness);
		// 骞傝繍绠?
		// 濡傛灉鏄?phong 妯″瀷鐨勮瘽:
		// auto reflect = reflect(light_dir, normal).normalized();
		// specular = ks.cwiseProduct(light.intensity / distance_square) *
		// std::pow(std::max(0.0f, reflect.dot(view_dir)) ,shiness)

		result_color += ambient + diffuse + specular;
	}
	//濡傛灉瀛樺湪澶氫釜light 灏卞皢杩欎簺鐨刲ight鐨勪笁涓彉閲忛兘鍔犺捣鏉?

	return result_color;
}

// 鍦?blinn-phong 鐨勫熀纭€涓?color 鍘?texture 閲囨牱
Vector3f texture_fragment_shader(const fragment_shader_payload& payload) {
	Vector3f texture_color = { 0,0,0 };
	if (payload.texture) {
		texture_color = payload.texture->getColor(payload.tex_coords);
	}

	Vector3f ka = Vector3f(0.005, 0.005, 0.005);
	Vector3f kd = texture_color;
	Vector3f ks = Vector3f(0.7937, 0.7937, 0.7937);

	auto l1 = light{ {20,20,20},{500,500,500} };
	auto l2 = light{ {-20,20,0},{500,500,500} };

	std::vector<light> lights = { l1, l2 };
	Vector3f amb_light_intensity{ 10,10,10 };

	float shiness = 150;
	Vector3f color = texture_color;
	Vector3f point = payload.view_pos;
	Vector3f normal = payload.normal;

	Vector3f result_color = { 0,0,0 };

	for (auto& light : lights) {
		Vector3f light_dir = (light.position - point).normalized();
		Vector3f view_dir = (eye_pos - point).normalized();
		Vector3f half_vector = (light_dir + view_dir).normalized();
		normal.normalize();

		float distance_square = (light.position - point).dot(light.position - point);
		Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
		Vector3f diffuse = kd.cwiseProduct(light.intensity / distance_square) * 
			std::max(0.0f, normal.dot(light_dir));
		Vector3f specular = ks.cwiseProduct(light.intensity / distance_square) *
			std::pow(std::max(0.0f, normal.dot(half_vector)), shiness);

		result_color += ambient + diffuse + specular;
	}

	return result_color;
}

// Bump Mapping 鍑瑰嚫璐村浘/娉曠嚎璐村浘鏄竴绉嶇敤绾圭悊鏉ユā鎷熻〃闈㈠嚬鍑哥粏鑺傜殑鎶€鏈紝涓嶆敼鍔ㄥ嚑浣曢《鐐?
// 鍙慨鏀圭潃鑹叉椂鐨勬硶绾匡紝浠庤€屼骇鐢熷嚬鍑哥殑瑙嗚鍋囪薄
Vector3f bump_fragment_shader(const fragment_shader_payload& payload) {
	Vector3f normal = payload.normal;
	normal.normalize();

	float x = normal.x();
	float y = normal.y();
	float z = normal.z();
	Vector3f t(x * y / sqrt(x * x + z * z), sqrt(x * x + z * z), z * y / sqrt(x * x + z * z));
	// 鍒囩嚎鐨勭簿纭绠楀簲璇ヤ粠涓夎褰㈤《鐐规暟鎹腑寰楀埌 浣嗘槸鍦╢ragmentShader涓彧鏈夌墖娈垫暟鎹?
	// t 鏄熀浜庢硶绾跨殑杩戜技鍒囩嚎璁＄畻
	// 浣跨敤浜嗗緢澶氬亣璁捐幏寰楀垏鍚戦噺
	Vector3f b = normal.cross(t); //cross product
	// 鍓垏绾?

	Matrix3f TBN;
	TBN << t.x(), b.x(), x,
		   t.y(), b.y(), y,
		   t.z(), b.z(), z;
	//T:Tangent 鍒囩嚎銆?B锛欱itangent 鍓垏绾?
	//3D涓瓨鍦ㄧ殑鏄垏骞抽潰锛屽湪杩欓噷浣跨敤 T B 涓や釜鍚戦噺鏉ヨ〃绀哄垏骞抽潰

	float u = payload.tex_coords.x();
	float v = payload.tex_coords.y();
	float w = payload.texture->width;
	float h = payload.texture->height;

	float kh = 0.2, kn = 0.1;

	auto norm0 = payload.texture->getColor(u, v, false).norm();
	float dU = kh * kn * (payload.texture->getColor(u + 1.0f / w, v, false).norm() -
		norm0);
	float dV = kh * kn * (payload.texture->getColor(u, v + 1.0f / h, false).norm() -
		norm0);
	// 璇ョ墖娈电殑绾圭悊鍧愭爣 uv 鍦ㄧ汗鐞嗕腑 UV 鏂瑰悜涓婄殑鍙樺寲鐜囷紝瀵兼暟

	Vector3f ln(-dU, -dV, 1);
	// ln:local normal 鍒囩嚎绌洪棿涓殑娉曞悜閲?
	normal = (TBN * ln).normalized();

	// 濡傛灉杩欎釜鐗囨鍦ㄧ汗鐞嗕腑鐨勫潗鏍囷紝闄勮繎鐨勯鑹叉病鏈夊彉鍖栵紝normal涔熷氨涓嶄細鍙戠敓鍙樺寲鍛€锛?

	return normal;
}

// 鍦╞ump mapping鐨勫熀纭€涓婂疄鐜癲isplacement mapping
// 鍦ㄦ硶绾挎壈鍔ㄧ殑鍩虹杩樹細淇敼涓夎闈笂鐨勯《鐐瑰€?
Vector3f displacement_fragment_shader(const fragment_shader_payload& payload) {
	
	Vector3f texture_color = { 0,0,0 };
	if (payload.texture) {
		texture_color = payload.texture->getColor(payload.tex_coords);
	}
	
	Vector3f ka = Vector3f(0.005, 0.005, 0.005);
	Vector3f kd = texture_color;
	Vector3f ks = Vector3f(0.7937, 0.7937, 0.7937);

	auto l1 = light{ {20,20,20},{500,500,500} };
	auto l2 = light{ {-20,20,0},{500,500,500} };

	std::vector<light> lights = { l1,l2 };
	Vector3f amb_light_intensity{ 10,10,10 };

	float p = 150;

	Vector3f point = payload.view_pos;
	Vector3f normal = payload.normal;

	float kh = 0.2, kn = 0.1;

	normal.normalize();
	float x = normal.x();
	float y = normal.y();
	float z = normal.z();
	Vector3f t(x * y / sqrt(x * x + z * z), sqrt(x * x + z * z), z * y / sqrt(x * x + z * z));
	Vector3f b = normal.cross(t);

	Matrix3f TBN;
	TBN << t.x(), b.x(), x,
		   t.y(), b.y(), y,
		   t.z(), b.z(), z;

	float u = payload.tex_coords.x();
	float v = payload.tex_coords.y();
	float w = payload.texture->width;
	float h = payload.texture->height;

	auto norm0 = payload.texture->getColor(u, v, false).norm();
	float dU = kh * kn * (payload.texture->getColor(u + 1.0f / w, v, false).norm() -
		norm0);
	float dV = kh * kn * (payload.texture->getColor(u, v + 1.0f / h, false).norm() -
		norm0);

	Vector3f ln(-dU, -dV, 1);
	normal = (TBN * ln).normalized();

	point += (kn * normal * payload.texture->getColor(u, v).norm());
	// 閲嶇偣鏄繖鍙?viewpos 鐨勪綅缃篃鏀瑰彉浜?

	Vector3f result_color = { 0,0,0 };
	for (auto& light : lights) {
		Vector3f light_dir = (light.position - point).normalized();
		Vector3f view_dir = (eye_pos - point).normalized();
		Vector3f half_vector = (light_dir + view_dir).normalized();
		normal.normalize();

		//璺濈骞虫柟锛屼綔涓哄厜绾胯“鍑忓洜瀛?
		float distance_square = (light.position - point).dot(light.position - point);

		Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
		Vector3f diffuse = kd.cwiseProduct(light.intensity / distance_square) *
			std::max(0.0f, normal.dot(light_dir));
		Vector3f specular = ks.cwiseProduct(light.intensity / distance_square) *
			std::pow(std::max(0.0f, normal.dot(half_vector)), p);

		result_color += ambient + diffuse + specular;
	}

	return result_color;
}

Vector3f pbr_fragment_shader(const fragment_shader_payload& payload) {

}

int main(int argc, const char** argv) {
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

	std::vector<Triangle*> TriangleList;

	float angle = 180;
	bool command_line = false;
	std::string filename = "output.png";

	objl::Loader Loader;
	std::string obj_path = "models/spot/";

	bool loadout = Loader.LoadFile("models/spot/spot_triangulated_good.obj");
	for (auto mesh : Loader.LoadedMeshes) {
		for (int i = 0; i < mesh.Vertices.size(); i += 3) {
			Triangle* t = new Triangle();
			for (int j = 0; j < 3; j++) {
				t->setVertex(j, Vector3f(mesh.Vertices[i+j].Position.X, mesh.Vertices[i + j].Position.Y, mesh.Vertices[i + j].Position.Z));
				t->setNormal(j, Vector3f(mesh.Vertices[i + j].Normal.X, mesh.Vertices[i + j].Normal.Y, mesh.Vertices[i + j].Normal.Z));
				t->setTexCoord(j, Vector2f(mesh.Vertices[i + j].TextureCoordinate.X, mesh.Vertices[i + j].TextureCoordinate.Y));
			}
			TriangleList.push_back(t);
		}
	}

	rst::rasterizer r(width, height);

	auto texture_path = "spot_texture.png";
	r.set_texture(Texture(obj_path + texture_path));

	std::function<Vector3f(fragment_shader_payload)> active_shader = texture_fragment_shader;

	if (argc >= 2) {
		command_line = true;
		filename = std::string(argv[1]);

		if (argc == 3 && std::string(argv[2]) == "texture")
		{
			std::cout << "Rasterizing using the texture shader\n";
			active_shader = texture_fragment_shader;
			texture_path = "spot_texture.png";
			r.set_texture(Texture(obj_path + texture_path));
		}
		else if (argc == 3 && std::string(argv[2]) == "normal")
		{
			std::cout << "Rasterizing using the normal shader\n";
			active_shader = normal_fragment_shader;
		}
		else if (argc == 3 && std::string(argv[2]) == "phong")
		{
			std::cout << "Rasterizing using the phong shader\n";
			active_shader = phong_fragment_shader;
		}
		else if (argc == 3 && std::string(argv[2]) == "bump")
		{
			std::cout << "Rasterizing using the bump shader\n";
			active_shader = bump_fragment_shader;
		}
		else if (argc == 3 && std::string(argv[2]) == "displacement")
		{
			std::cout << "Rasterizing using the bump shader\n";
			active_shader = displacement_fragment_shader;
		}
	}

	r.set_vertex_shader(vertex_shader);
	r.set_fragment_shader(active_shader);

	int key = 0;
	int frame_count = 0;
	if (command_line) {
		r.clear(rst::Buffers::Color | rst::Buffers::Depth
			| rst::Buffers::SampleDepth | rst::Buffers::SampleColor);

		r.set_model(get_model_matrix(angle, 2.5));
		r.set_view(get_view_matrix(eye_pos));
		r.set_projection(get_projection_matrix(45, 0.1, 50));

		r.draw(TriangleList);

		cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
		image.convertTo(image, CV_8UC3, 1.0f);
		cv::imwrite(filename, image);

		return 0;
	}


	std::vector<Eigen::Vector3f> pos
	{
			{2, 0, -2},
			{0, 2, -2},
			{-2, 0, -2},
			{3.5, -1, -5},
			{2.5, 1.5, -5},
			{-1, 0.5, -5}
	};

	std::vector<Eigen::Vector3i> ind
	{
			{0, 1, 2},
			{3, 4, 5}
	};

	std::vector<Eigen::Vector3f> cols
	{
			{217.0, 238.0, 185.0},
			{217.0, 238.0, 185.0},
			{217.0, 238.0, 185.0},
			{185.0, 217.0, 238.0},
			{185.0, 217.0, 238.0},
			{185.0, 217.0, 238.0}
	};

	auto pos_id = r.load_positions(pos);
	auto ind_id = r.load_indices(ind);
	auto col_id = r.load_colors(cols);

	while (key != 27) {
		r.clear(rst::Buffers::Color | rst::Buffers::Depth
			| rst::Buffers::SampleDepth | rst::Buffers::SampleColor);

		//姣忎釜椤剁偣涓€娆?椤剁偣鐫€鑹查樁娈?start
		r.set_view(get_view_matrix(eye_pos));
		r.set_projection(get_projection_matrix(45, 0.1, 50));

		r.set_model(get_model_matrix(angle, 2.5));
		r.draw(TriangleList);

		/*r.set_model(get_model_matrix(0, 1));
		r.draw(pos_id, ind_id, col_id, rst::Primitive::Triangle);*/

		cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
		image.convertTo(image, CV_8UC3, 1.0f);
		cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
		cv::imshow("image", image);
		//OpenCV 涓槸 BGR 鐨勯『搴?

		key = cv::waitKey();
		std::cout << "frame count: " << frame_count++ << '\n';

		if (key == 'a') {
			angle += 10;
		}
		else if(key == 'd') {
			angle -= 10;
		}
		else if (key == 'w') { //涓婄澶?
			eye_pos << eye_pos.x(), eye_pos.y() + 0.5, eye_pos.z();
		}
		else if (key == 's') { //涓嬬澶?
			eye_pos << eye_pos.x(), eye_pos.y() - 0.5, eye_pos.z();
		}
	}

	return 0;
}

