//
// Created by robin on 11/11/15.
//

#include <check.h>
#include "newfs.h"


void setup() {
    struct newfs_state *newfs_internal_state;
    newfs_internal_state = malloc(sizeof(struct newfs_state));
    newfs_internal_state->logfile = init_log_file();
    init_fs();
}

void teardown() {
    shutdown_fs();
    // Delete the database file.
    unlink(DATABASE_NAME);
}

// ---- Check get_fcb. ----
START_TEST (check_get_fcb_root)
    {
        struct fcb test_struct;
        get_fcb(&root_object.id, &test_struct);
        ck_assert_int_eq(0, test_struct.name_len);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST


START_TEST (check_get_fcb_sub_folder)
    {
        struct fcb test_struct;
        initialize_element(&test_struct, true);

        mode_t mode = S_IRUSR;

        newfs_mkdir("/test1", NULL);
        struct fcb test1;
        struct fcb test1_get;
        resolve_path(&test1, "/test1");
        get_fcb(&test1.uuid, &test1_get);
        int memcm = memcmp(&test1, &test1_get, sizeof(struct fcb));
        ck_assert_msg(memcm == 0,
                        "Resolved fcb and get_fcb not identical.");

        int rc = newfs_mkdir("/test1/test1", NULL);
        ck_assert_msg(rc == 0, "Could not create nested test folder.");
        struct fcb test2;
        struct fcb test2_get;
        rc = resolve_path(&test2, "/test1/test1");
        ck_assert_msg(rc == 0, "Could not resolve nested test folder.");
        get_fcb(&test2.uuid, &test2_get);
        ck_assert_msg(memcmp(&test2, &test2_get, sizeof(struct fcb)) == 0,
                      "Resolved fcb and get_fcb not identical.");

        newfs_mkdir("/test2", NULL);
        struct fcb test3;
        struct fcb test3_get;
        resolve_path(&test3, "/test2");
        get_fcb(&test3.uuid, &test3_get);
        ck_assert_msg(memcmp(&test3, &test3_get, sizeof(struct fcb)) == 0,
                      "Resolved fcb and get_fcb not identical.");

        // Clean-up.
        newfs_rmdir("/test1/test1");
        newfs_rmdir("/test1");
        newfs_rmdir("/test2");
    }
END_TEST

// ---- Check resolve path. ----
START_TEST (check_resolve_path_root)
    {
        struct fcb test_struct;
        resolve_path(&test_struct, "/");

        struct fcb root_fcb;
        get_fcb(&root_object.id, &root_fcb);
        int rc = memcmp(&test_struct, &root_fcb, sizeof(struct fcb));

        ck_assert_int_eq(0, rc);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_resolve_path_ENOENT)
    {
        struct fcb test_fcb;
        int rc = resolve_path(&test_fcb, "/123");

        ck_assert_int_eq(rc, ENOENT);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

/*
 * Tests:
 *  - mkdir
 *  - resolve_path from root
 *  Uses:
 *  - rmdir
 */
START_TEST (check_resolve_path_sub_folder_from_root)
    {
        char test_path[] = "/test";
        // Check folder does not exist.
        struct fcb first_fcb;
        int rc = resolve_path(&first_fcb, test_path);
        ck_assert_msg(rc == ENOENT, "Not found error code not returned from empty root.");

        // Create a folder.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        rc = newfs_mkdir(test_path, mode);
        ck_assert_msg(rc == 0, "Error code incorrectly returned when creating folder.");

        // Check that folder exists.
        struct fcb test_fcb;
        rc = resolve_path(&test_fcb, test_path);
        ck_assert_msg(rc == 0, "Error code incorrectly returned when resolving path.");

        char name[test_fcb.name_len]; get_name(&test_fcb, name);
        char path[test_fcb.path_len]; get_path(&test_fcb, path);
        ck_assert(strcmp(path, test_path) == 0);
        ck_assert(strcmp(name, "test") == 0);
        ck_assert(test_fcb.size == 0);

        // Delete folder.
        newfs_rmdir(test_path);

        // Check that folder is deleted.
        rc = resolve_path(&test_fcb, test_path);
        ck_assert_int_eq(rc, ENOENT);

        // Invalid; part of path not folder.
        struct fcb root;
        resolve_path(&root, "/");
        rc = newfs_create("/test", NULL, NULL);
        resolve_path(&root, "/");
        rc = resolve_path(&test_fcb, "/test");
        rc = resolve_path(&test_fcb, "/test/test");
        ck_assert_msg(rc == ENOTDIR, "Not a directory error not returned.");

        newfs_unlink("/test");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

/*
 * Tests
 *  - is_dir
 *  - initialize_element
 */
START_TEST(check_is_dir)
    {
        struct fcb test_dir;
        initialize_element(&test_dir, true);
        ck_assert(is_dir(&test_dir));

        initialize_element(&test_dir, false);
        ck_assert(!is_dir(&test_dir));

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

/*
 * Tests
 *  - initialize_element for common fields
 */
START_TEST(check_initialize_element_common)
    {
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, true);
        ck_assert(dir_fcb.size == 0);
        ck_assert(dir_fcb.name_len == 0);
        ck_assert(dir_fcb.path_len == 0);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

/*
 * Tests
 *  - initialize_element for common fields
 */
START_TEST(check_initialize_element_dir)
    {
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, true);
        ck_assert_msg(S_ISDIR(dir_fcb.mode), "FCB's mode is not of directory type.");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

/*
 * Tests
 *  - initialize_element for common fields
 */
START_TEST(check_initialize_element_file)
    {
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, false);
        ck_assert_msg(S_ISREG(dir_fcb.mode), "FCB's mode is not of (regular) file type.");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_set_get_name)
    {
        char *test_name = "TestName";
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, true);
        set_name(&dir_fcb, test_name);
        ck_assert(dir_fcb.name_len == (strlen(test_name)));

        char res_n[dir_fcb.name_len + 1];
        get_name(&dir_fcb, res_n);
        ck_assert(dir_fcb.name_len == (strlen(res_n)));
        ck_assert(strcmp(test_name, res_n) == 0);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_set_get_path)
    {
        char *test_path = "/sample/path/to/magic";
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, true);
        set_path(&dir_fcb, test_path);
        ck_assert(dir_fcb.path_len == (strlen(test_path)));

        char res_n[dir_fcb.path_len + 1];
        get_path(&dir_fcb, res_n);
        ck_assert(dir_fcb.path_len == (strlen(res_n)));
        ck_assert(strcmp(test_path, res_n) == 0);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_set_get_data)
    {
        char test_data[] = "TestData123longername?";
        struct fcb dir_fcb;
        initialize_element(&dir_fcb, true);
        set_data(&dir_fcb, test_data, strlen(test_data) + 1);
        ck_assert(dir_fcb.size == ((strlen(test_data)) + 1));

        char res_n[dir_fcb.name_len + 1];
        get_data(&dir_fcb, res_n);
        ck_assert(dir_fcb.size == ((strlen(res_n)) + 1));
        ck_assert(strcmp(test_data, res_n) == 0);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_dat_truncate)
    {
        // Check valid inputs; non-boundary.
        struct fcb test_fcb;
        initialize_element(&test_fcb, false);
        struct fcb *test_ref = &test_fcb;
        char test_data[11] = "0123456789";
        set_data(test_ref, test_data, 11);

        int rc = dat_truncate(&test_fcb, 6);
        ck_assert_msg(rc == 0, "Return code is not 0 for valid operation, truncate middle.");
        ck_assert(test_fcb.size == 6);
        char new_data[6];
        get_data(test_ref, new_data);
        ck_assert_msg(memcmp(new_data, test_data, 6) == 0, "The truncated data does not have correct content.");

        // Boundary, zero.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_truncate(test_ref, 0);
        ck_assert_msg(test_fcb.size == 0, "The size is not set correctly for size 0.");
        ck_assert_msg(rc == 0, "Return code is not 0 for valid operation, bound 0.");

        // Boundary, full size.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_truncate(test_ref, 11);
        ck_assert_msg(test_fcb.size == 11, "The size is not set correctly when truncated with full size.");
        ck_assert_msg(rc == 0, "Return code is not 0 for valid operation, bound full size.");

        // Check invalid input.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_truncate(test_ref, -1);
        ck_assert_msg(rc == 1, "Return code is not indicating error for error; negative number.");
        rc = dat_truncate(test_ref, 12);
        ck_assert_msg(rc == 1, "Return code is not indicating error for error; bigger than size.");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_dat_del_chunk)
    {
        // Set up data.
        struct fcb test_fcb;
        initialize_element(&test_fcb, false);
        struct fcb *test_ref = &test_fcb;
        char test_data[11] = "0123456789";
        set_data(test_ref, test_data, 11);

        // Check valid input; non boundary.
        int rc = dat_del_chunk(test_ref, 2, 3);
        ck_assert_msg(rc == 0, "Return value from non-bound check is incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 8, "New size field is not correctly set.");
        char res_dat[8];
        get_data(test_ref, res_dat);
        ck_assert_msg(memcmp(res_dat, "0156789", 8) == 0, "Middle elements not correctly deleted.");

        // Check boundary; 0 index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, 0, 2);
        ck_assert_msg(rc == 0, "Return value from 0 index bound check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 9, "New size field is not correctly set; 0 bound test.");
        char res_dat2[9];
        get_data(test_ref, res_dat2);
        ck_assert_msg(memcmp(res_dat2, "23456789", 9) == 0, "Leading elements not correctly deleted.");


        // Check boundary; top index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, 10, 1);
        ck_assert_msg(rc == 0, "Return value from max index bound check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 10, "New size field is not correctly set; max index bound test.");
        char res_dat3[10];
        get_data(test_ref, res_dat3);
        ck_assert_msg(memcmp(res_dat3, "0123456789", 10) == 0, "Trailing elements not correctly deleted.");

        // Check boundary; top index, several.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, 8, 3);
        ck_assert_msg(rc == 0, "Return value from max index bound-several- check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 8, "New size field is not correctly set; max index bound test several.");
        char res_dat4[8];
        get_data(test_ref, res_dat4);
        ck_assert_msg(memcmp(res_dat4, "01234567", 8) == 0, "Trailing elements not correctly deleted.");


        // Invalid data tests; start index negative.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, -1, 3);
        ck_assert_msg(rc == 1, "Return value from negative start index incorrect, 1 expected.");

        // Invalid data tests; larger than max index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, 11, 3);
        ck_assert_msg(rc == 1, "Return value from too large start index incorrect, 1 expected.");

        // Invalid, starting in valid area, too large size.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_del_chunk(test_ref, 9, 3);
        ck_assert_msg(rc == 1, "Return value from size out of bounds incorrect, 1 expected.");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_dat_insert_chunk)
    {
        // Set up data.
        struct fcb test_fcb;
        initialize_element(&test_fcb, false);
        struct fcb *test_ref = &test_fcb;
        char test_data[11] = "0123456789";
        set_data(test_ref, test_data, 11);

        // Check valid input; non-boundary.
        int rc = dat_insert_chunk(test_ref, 2, "iii", 3);
        ck_assert_msg(rc == 0, "Return value from non-bound check is incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 14, "New size field is not correctly set.");
        char res_dat[14];
        get_data(test_ref, res_dat);
        ck_assert_msg(memcmp(res_dat, "01iii23456789", 14) == 0, "Middle elements not correctly inserted.");

        // Check boundary; 0 index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, 0, "--", 2);
        ck_assert_msg(rc == 0, "Return value from 0 index bound check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 13, "New size field is not correctly set; 0 bound test.");
        char res_dat2[13];
        get_data(test_ref, res_dat2);
        ck_assert_msg(memcmp(res_dat2, "--0123456789", 13) == 0, "Leading elements not correctly inserted.");


        // Check boundary; top index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, 10, "--", 2);
        ck_assert_msg(rc == 0, "Return value from max index bound check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 13, "New size field is not correctly set; max index bound test.");
        char res_dat3[13];
        get_data(test_ref, res_dat3);
        ck_assert_msg(memcmp(res_dat3, "0123456789--", 13) == 0, "Trailing elements not correctly deleted.");

        // Check boundary; append, top index.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, 11, "--", 2);
        ck_assert_msg(rc == 0, "Return value from max index bound check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 13, "New size field is not correctly set; max index append.");
        char res_dat4[13];
        get_data(test_ref, res_dat4);
        ck_assert_msg(memcmp(res_dat4, "0123456789\0--", 13) == 0,
                      "Trailing elements not correctly inserted, top element append.");


        // Check boundary; top index append -1.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, -1, "hello", 5);
        ck_assert_msg(rc == 0, "Return value from max index bound-several- check incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 16, "New size field is not correctly set; max index bound test append -1.");
        char res_dat5[16];
        get_data(test_ref, res_dat5);
        ck_assert_msg(memcmp(res_dat5, "0123456789\0hello", 16) == 0, "Trailing elements not correctly appended.");

        // Check boundary; zero length insert.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, 3, "hello", 0);
        ck_assert_msg(rc == 0, "Return value from zero-length insert incorrect, 0 expected.");
        ck_assert_msg(test_fcb.size == 11, "New size field is not correctly set; zero-insert.");
        char res_dat6[11];
        get_data(test_ref, res_dat6);
        ck_assert_msg(memcmp(res_dat6, "0123456789", 11) == 0, "Zero-insert caused error with data.");

        // Check invalid input, < 0 && != -1
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, -2, "hello", 5);
        ck_assert_msg(rc == 1, "Return value from incorrect index < 0 && != -1 incorrect, 1 expected.");

        // Check invalid input, index > size.
        set_data(test_ref, test_data, 11); // Reset data.
        rc = dat_insert_chunk(test_ref, 12, "hello", 5);
        ck_assert_msg(rc == 1, "Return value from incorrect index > size incorrect, 1 expected.");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_tokenize_path)
    {
        char **tokens;
        int count;
        int rc = 0;
        rc = tokenize_path("asd", &tokens, &count);
        ck_assert(rc != ENOENT);
        ck_assert(count == 1);
        ck_assert(strcmp(tokens[0], "asd") == 0);


        rc = tokenize_path("asd//last", &tokens, &count);
        ck_assert(rc == ENOENT);

        rc = tokenize_path("/asd/last/", &tokens, &count);

        ck_assert(rc != ENOENT);
        ck_assert(strcmp(tokens[0], "") == 0);
        ck_assert(strcmp(tokens[1], "asd") == 0);
        ck_assert(strcmp(tokens[2], "last") == 0);
        ck_assert(strcmp(tokens[3], "") == 0);
        ck_assert(count == 4);

        rc = tokenize_path("/", &tokens, &count);

        ck_assert(rc != ENOENT);
        ck_assert(count == 2);
        ck_assert(strcmp(tokens[0], "") == 0);
        ck_assert(strcmp(tokens[1], "") == 0);

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_separate_path)
    {
        // From root.
        char path[] = "/hello";
        int index;
        separate_path(path, &index);
        ck_assert_msg(strcmp(path, "") == 0, "The root object is equivalent to empty string.");
        ck_assert_msg(strcmp(&path[index], "hello") == 0, "The file object's name is not extracted correctly.");

        // In sub-folder.
        char path2[] = "/test/a/b/c";
        separate_path(path2, &index);
        ck_assert_msg(strcmp(path2, "/test/a/b") == 0, "The parent path is not extracted correctly.");
        ck_assert_msg(strcmp(&path2[index], "c") == 0,
                      "The file object's name is not extracted correctly (non-root parent case.");

        // Trailing slash.
        char path3[] = "/test/a/b/c/";
        separate_path(path3, &index);
        ck_assert_msg(strcmp(path3, "/test/a/b") == 0, "The parent path is not extracted correctly.(trailing slash)");
        ck_assert_msg(strcmp(&path3[index], "c") == 0,
                      "The file object's name is not extracted correctly. (trailing slash)");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_resolve_ancestor)
    {
        // Set up simple file folders.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;;
        char creat_path_one[] = "/one";
        char creat_path_two[] = "/one/two";
        char creat_path_three[] = "/one/two/three";
        newfs_mkdir(creat_path_one, mode);
        newfs_mkdir(creat_path_two, mode);
        newfs_mkdir(creat_path_three, mode);

        // Test for self.
        struct fcb ancestor;
        char path[] = "/one/two/three";
        int rc = resolve_ancestor(&ancestor, path, 0);
        ck_assert_msg(rc == 0, "Error code for self not 0.");
        char name1[ancestor.name_len];
        char path1[ancestor.path_len];
        get_name(&ancestor, name1);
        get_path(&ancestor, path1);
        ck_assert_msg(strcmp(name1, "three") == 0, "Self name is not extracted correctly.");
        ck_assert_msg(strcmp(path1, "/one/two/three") == 0, "Self path is not extracted correctly.");

        // Calculate direct parent.
        char path_copy[] = "/one/two/three";
        rc = resolve_ancestor(&ancestor, path_copy, 1);
        ck_assert_msg(rc == 0, "Error code for parent not 0.");
        char name2[ancestor.name_len];
        char path2[ancestor.path_len];
        get_name(&ancestor, name2);
        get_path(&ancestor, path2);
        ck_assert_msg(strcmp(name2, "two") == 0, "Parent is not extracted correctly.");
        ck_assert_msg(strcmp(path2, "/one/two") == 0, "Parent path is not extracted correctly.");

        // Calculate parent's parent.
        char path_copy2[] = "/one/two/three";
        rc = resolve_ancestor(&ancestor, path_copy2, 2);
        ck_assert_msg(rc == 0, "Error code for parent not 0.");
        char name3[ancestor.name_len];
        char path3[ancestor.path_len];
        get_name(&ancestor, name3);
        get_path(&ancestor, path3);
        ck_assert_msg(strcmp(name3, "one") == 0, "Parent is not extracted correctly.");
        ck_assert_msg(strcmp(path3, "/one") == 0, "Parent path is not extracted correctly.");

        // Check boundary, should return root.
        char path_copy3[] = "/one/two/three";
        rc = resolve_ancestor(&ancestor, path_copy3, 3);
        ck_assert_msg(rc == 0, "Error code for root not returned.");
        char name5[ancestor.name_len];
        char path5[ancestor.path_len];
        get_name(&ancestor, name5);
        get_path(&ancestor, path5);
        ck_assert_msg(strcmp(name5, "") == 0, "Root (boundary-normal) is not extracted correctly.");
        ck_assert_msg(strcmp(path5, "") == 0, "Root (boundary-normal) is not extracted correctly.");

        // Check boundary, number too high, should return root.
        char path_copy4[] = "/one/two/three";
        rc = resolve_ancestor(&ancestor, path_copy4, 4);
        ck_assert_msg(rc == 0, "Error code for root not returned.");
        char name4[ancestor.name_len];
        char path4[ancestor.path_len];
        get_name(&ancestor, name4);
        get_path(&ancestor, path4);
        ck_assert_msg(strcmp(name4, "") == 0, "Root (boundary) is not extracted correctly.");
        ck_assert_msg(strcmp(path4, "") == 0, "Root (boundary) is not extracted correctly.");

        // Check invalid.
        // Check boundary, number too high, should return root.
        char path_copy5[] = "/one/two/three";
        rc = resolve_ancestor(&ancestor, path_copy5, -1);
        ck_assert_msg(rc != 0, "Error code for root not returned.");

        rc = resolve_ancestor(&ancestor, "/hello/what", 0);
        ck_assert_msg(rc == ENOENT, "Error code for object not found not correct.");

        // Clean-up.
        newfs_rmdir("/one/two/three");
        newfs_rmdir("/one/two");
        newfs_rmdir("/one");

        struct fcb root;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_get_name_from_fcb)
    {
        // Create a number of directories.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        char creat_path_one[] = "/hello";
        char creat_path_two[] = "/hello1";
        char creat_path_three[] = "/hello2";
        newfs_mkdir(creat_path_one, mode);
        newfs_mkdir(creat_path_two, mode);
        newfs_mkdir(creat_path_three, mode);

        struct fcb root;
        resolve_path(&root, "/");

        // Check valid input.
        struct fcb current_dir;
        int rc = get_fcb_from_name(&root, "hello", &current_dir);
        ck_assert_msg(rc == 0, "Error message returned when requesting hello.");
        char name[current_dir.name_len];
        char path[current_dir.path_len];
        get_name(&current_dir, name);
        get_path(&current_dir, path);
        ck_assert_msg(rc == 0, "Return code from first folde wrongly indicated error.");
        ck_assert_msg(strcmp(path, "/hello") == 0, "Path of retrived FCB incorrect; first.");
        ck_assert_msg(strcmp(name, "hello") == 0, "Name of retrived FCB incorrect; first.");

        rc = get_fcb_from_name(&root, "hello1", &current_dir);
        get_name(&current_dir, name);
        get_path(&current_dir, path);
        ck_assert_msg(rc == 0, "Return code from first folde wrongly indicated error.");
        ck_assert_msg(strcmp(path, "/hello1") == 0, "Path of retrived FCB incorrect; second.");
        ck_assert_msg(strcmp(name, "hello1") == 0, "Name of retrived FCB incorrect; second.");

        rc = get_fcb_from_name(&root, "hello2", &current_dir);
        ck_assert_msg(rc == 0, "Return code from first folde wrongly indicated error.");
        get_name(&current_dir, name);
        get_path(&current_dir, path);
        ck_assert_msg(strcmp(path, "/hello2") == 0, "Path of retrived FCB incorrect; third.");
        ck_assert_msg(strcmp(name, "hello2") == 0, "Name of retrived FCB incorrect; third.");

        // Check invalid input
        rc = get_fcb_from_name(&root, "hello123", &current_dir);
        ck_assert_msg(rc == ENOENT, "No error message is returned when element is not found.");

        // Clean-up.
        newfs_rmdir(creat_path_one);
        newfs_rmdir(creat_path_two);
        newfs_rmdir(creat_path_three);
    }
END_TEST

START_TEST(check_remove_UUID_fr_dir)
    {
        struct fcb root;
        resolve_path(&root, "/");
        uuid_t uuid_tmp;
        memcpy(uuid_tmp, root.uuid, sizeof(uuid_t));
        struct fcb tmp_fcb;
        struct fcb tmp_fcb2;

        // Valid; one UUID present.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        newfs_mkdir("/hello", mode);
        resolve_path(&root, "/");
        resolve_path(&tmp_fcb, "/hello");
        ck_assert(root.size == KEY_SIZE);

        int rc = remove_UUID_from_dir(&root, &tmp_fcb.uuid);
        get_record_size(&root_object.id, &tmp_fcb, sizeof(struct fcb));
        ck_assert(memcmp(&uuid_tmp, &root_object.id, sizeof(uuid_t)) == 0);
        ck_assert_msg(rc == 0, "Error code returned, first test.");
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Size of root not correct after deletion.");

        struct fcb temp;
        rc = get_fcb_from_name(&root, "hello", &temp);
        ck_assert_msg(rc == ENOENT, "Element still present after deleting UUID.");

        // Valid; multiple directories.
        newfs_mkdir("/hello1", mode);
        newfs_mkdir("/hello2", mode);
        resolve_path(&root, "/");
        ck_assert(root.size == 2 * KEY_SIZE);

        resolve_path(&tmp_fcb, "/hello1");
        resolve_path(&tmp_fcb2, "/hello2");

        rc = remove_UUID_from_dir(&root, &tmp_fcb.uuid);
        ck_assert_msg(rc == 0, "Error code returned, 2-1 test.");
        rc = remove_UUID_from_dir(&root, &tmp_fcb2.uuid);
        ck_assert_msg(rc == 0, "Error code returned, 2-2 test.");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Size of root not correct after second test deletion.");


        // Clean-up.
        newfs_rmdir("/hello");
        newfs_rmdir("/hello1");
        newfs_rmdir("/hello2");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_rm_element_fr_dir)
    {
        struct fcb root;
        struct fcb tmp_fcb;
        struct fcb tmp_fcb2;

        // Valid; one directory present.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        newfs_mkdir("/hello", mode);
        resolve_path(&root, "/");
        resolve_path(&tmp_fcb, "/hello");
        ck_assert(root.size == KEY_SIZE);

        int rc = rm_element_from_directory(&tmp_fcb, "/hello", true);
        ck_assert_msg(rc == 0, "Error message returned, deleting one directory");
        rc = resolve_path(&tmp_fcb, "/hello");
        ck_assert_msg(rc == ENOENT, "Element was still found after deletion.");
        newfs_rmdir("/hello");
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Size of root incorrect after first test.");


        // Valid; one file present.
        newfs_create("/hello.txt", mode, NULL);
        resolve_path(&root, "/");
        ck_assert(root.size == KEY_SIZE);
        resolve_path(&tmp_fcb, "/hello.txt");
        rc = rm_element_from_directory(&tmp_fcb, "/hello.txt", false);
        ck_assert_msg(rc == 0, "Error message returned, deleting one directory");
        rc = resolve_path(&tmp_fcb, "/hello.txt");
        ck_assert_msg(rc == ENOENT, "Element was still found after deletion.");
        newfs_unlink("/hello.txt");

        // Invalid; remove file on directory.
        newfs_mkdir("/test", mode);
        resolve_path(&tmp_fcb, "/test");
        rc = rm_element_from_directory(&tmp_fcb, "/test", false);
        ck_assert_msg(rc == EISDIR, "Wrong error when file deletion on directory");
        rc = resolve_path(&tmp_fcb, "/test");
        ck_assert_msg(rc == 0, "Dir was not found after file deletion on dir.");
        rc = resolve_path(&root, "/");
        ck_assert_msg(root.size == KEY_SIZE, "Root size incorrect after file del on dir.");
        newfs_rmdir("/test");

        // Invalid; remove directory on file.
        newfs_create("/test.txt", mode, NULL);
        resolve_path(&tmp_fcb, "/test.txt");
        rc = rm_element_from_directory(&tmp_fcb, "/test.txt", true);
        ck_assert_msg(rc == ENOTDIR, "Wrong error when dir deletion on file.");
        rc = resolve_path(&tmp_fcb, "/test.txt");
        ck_assert_msg(rc == 0, "File was not found after dir deletion on file.");
        rc = resolve_path(&root, "/");
        ck_assert_msg(root.size == KEY_SIZE, "Root size incorrect after file del on dir.");
        newfs_unlink("/test.txt");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_getattr_chown_chmod)
    {
        // Valid; Change permissions of file.
        mode_t mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        newfs_create("/testfile", mode, NULL);

        struct stat getattr_out;
        newfs_getattr("/testfile", &getattr_out);
        ck_assert_msg(getattr_out.st_mode == mode, "Initial permissions are incorrect");
        mode = S_IFREG | S_IRUSR | S_IWUSR;
        int rc = newfs_chmod("/testfile", mode);
        newfs_getattr("/testfile", &getattr_out);
        ck_assert_msg(rc == 0, "chmod existing fil returned error.");
        ck_assert_msg(getattr_out.st_mode == mode, "Initial permissions are incorrect");

        // Valid; Change owner.
        struct fcb tmp_fcb;
        resolve_path(&tmp_fcb, "/testfile");
        ck_assert_msg(tmp_fcb.uid == getuid());
        ck_assert_msg(tmp_fcb.gid == getgid());

        rc = newfs_chown("/testfile", 100, 200);
        ck_assert_msg(rc == 0, "chown with expected vals returned error.");
        resolve_path(&tmp_fcb, "/testfile");
        ck_assert_msg(tmp_fcb.uid == 100, "Wrong uid after change.");
        ck_assert_msg(tmp_fcb.gid == 200, "Wrong gid after change.");
        rc = newfs_getattr("/testfile", &getattr_out);
        ck_assert_msg(rc == 0, "gettattr after chown resulted in unexpected error.");
        ck_assert_msg(getattr_out.st_uid == 100, "Wrong uid after change. (getattr)");
        ck_assert_msg(getattr_out.st_gid == 200, "Wrong gid after change. (getattr)");

        // Boundary; if uid or gid are -1, don't change.
        rc = newfs_chown("/testfile", -1, -1);
        ck_assert_msg(rc == 0, "chown with -1 returned error.");
        resolve_path(&tmp_fcb, "/testfile");
        ck_assert_msg(tmp_fcb.uid == 100, "uid change after -1 input.");
        ck_assert_msg(tmp_fcb.gid == 200, "gid change after -1 input.");

        // Valid; update utime.
        struct utimbuf utimbuf;
        time(&utimbuf.actime);
        time(&utimbuf.modtime);
        rc = newfs_getattr("/testfile", &getattr_out);
        time_t mtime = getattr_out.st_mtim.tv_sec;
        time_t atime = getattr_out.st_atim.tv_sec;

        rc = newfs_utime("/testfile", &utimbuf);
        ck_assert_msg(rc == 0, "Updating utime resulted in error.");
        rc = newfs_getattr("/testfile", &getattr_out);
        time_t mtime_2 = getattr_out.st_mtim.tv_sec;
        time_t atime_2 = getattr_out.st_atim.tv_sec;
        ck_assert_msg(mtime <= mtime_2, "mtime not correctly updated.");
        ck_assert_msg(atime <= atime_2, "atime not correctly updated.");
    }
END_TEST

START_TEST(check_open)
    {
        // Valid; open existing file.
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        newfs_create("/testfile", mode, NULL);
        int rc = newfs_open("/testfile", NULL);
        ck_assert_msg(rc == 0, "Open of existing file failed.");

        // Invalid; open non-existent file.
        rc = newfs_open("/testfile123", NULL);
        ck_assert_msg(rc == -ENOENT, "Open of non-existing file got wrong error code.");

    }
END_TEST

START_TEST(check_read_write)
    {
        struct fcb tmp_fcb;
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

        // Create a file.
        newfs_create("/testfile", mode, NULL);

        // Valid; Write to the file; zero offset.
        char test_input[11] = "0123456789";
        int rc = newfs_write("/testfile", test_input, 11, 0, NULL);
        ck_assert_msg(rc == 11, "Writing to file failed.");
        rc = resolve_path(&tmp_fcb, "/testfile");
        ck_assert_msg(rc == 0, "Could not retrieve file after writing to it.");

        char check_buffer[11];
        get_data(&tmp_fcb, check_buffer);
        ck_assert_msg(strcmp(check_buffer, test_input) == 0, "Stored and test input do not match.");

        // Valid; Read from file; zero offset.
        memset(check_buffer, 0, 11);
        rc = newfs_read("/testfile", check_buffer, 11, 0, NULL);
        ck_assert_msg(rc == 11, "Reading from first file not successful.");
        ck_assert_msg(strcmp(check_buffer, test_input) == 0, "Stored and test input do not match.(newfs_read)");

        // Valid; Read subset; zero offset.
        memset(check_buffer, 0, 11);
        rc = newfs_read("/testfile", check_buffer, 5, 0, NULL);
        ck_assert_msg(rc == 5, "Reading from first file not successful. (newfs_read zero subset)");
        ck_assert_msg(memcmp(check_buffer, test_input, 5) == 0, "Stored and test input do not match.(newfs_read - subset)");

        // Valid; Read subset; non-zero offset.
        memset(check_buffer, 0, 11);
        rc = newfs_read("/testfile", check_buffer, 5, 3, NULL);
        ck_assert_msg(rc == 5, "Reading from first file not successful. (newfs_read zero subset)");
        ck_assert_msg(memcmp(check_buffer, &test_input[3], 5) == 0, "Stored and test input do not match.(newfs_read -  non-zero subset)");

        // TODO test different write operations.

        // Invalid; Non-existing.
        rc = newfs_read("test235", check_buffer, 4, 11, NULL);
        ck_assert_msg(rc == -ENOENT, "If entry doesn't exist incorrect error returned.");

        newfs_mkdir("/testfolder", mode);
        rc = newfs_read("/testfolder", check_buffer, 4, 11, NULL);
        ck_assert_msg(rc == -EISDIR, "If entry is directory incorrect error returned.");

    }
END_TEST

START_TEST(check_create_and_unlink)
    {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        struct fcb root;
        struct fcb tmp_fcb;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root is not empty.");

        // Valid; one file.
        int rc = resolve_path(&tmp_fcb, "/test.txt");
        ck_assert_msg(rc == ENOENT, "File already exists.");
        rc = newfs_create("/test.txt", mode, NULL);
        ck_assert_msg(rc == 0, "File not created (first)");
        rc = resolve_path(&tmp_fcb, "/test.txt");
        ck_assert_msg(rc == 0, "File not created.");

        // Checking name and path.
        char temp_name[tmp_fcb.name_len];
        get_name(&tmp_fcb, temp_name);
        ck_assert_msg(strcmp(temp_name, "test.txt") == 0, "Name is not set correctly");
        char temp_path[tmp_fcb.path_len];
        get_path(&tmp_fcb, temp_path);
        ck_assert_msg(strcmp(temp_path, "/test.txt") == 0, "Path is not set correctly");

        rc = newfs_unlink("/test.txt");
        ck_assert_msg(rc == 0, "File not deleted successfully.");
        rc = resolve_path(&tmp_fcb, "/test.txt");
        ck_assert_msg(rc == ENOENT, "File was still found after deletion.");

        // Invalid; delete non-existent file.
        rc = newfs_unlink("/test123");
        ck_assert_msg(rc == -ENOENT, "Non exist file error incorrect.");

        // Invalid; delete non-file.
        newfs_mkdir("/test", mode);
        rc = newfs_unlink("/test");
        ck_assert_msg(rc == -EISDIR, "directory delete error incorrect.");
        newfs_rmdir("/test");

        // Invalid; create file where file already exists.
        rc = newfs_create("/test", mode, NULL);
        ck_assert_msg(rc == 0, "test file not created.");
        rc = newfs_create("/test", mode, NULL);
        ck_assert_msg(rc == -EEXIST, "Same name test could create file again.");
        newfs_unlink("/test");

        // Invalid; parent dir not folder.
        rc = newfs_create("/test", mode, NULL);
        rc = newfs_create("/test/test1", mode, NULL);
        ck_assert_msg(rc == -ENOTDIR, "Parent not dir error incorrect.");
        newfs_unlink("/test");

        // Invalid; create on incorrect path.
        rc = newfs_create("/test/test1", mode, NULL);
        ck_assert_msg(rc = -ENOENT, "Create on non-existent path error incorrect.");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST

START_TEST(check_mkdir)
    {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        struct fcb root;
        struct fcb tmp_dir;
        resolve_path(&root, "/");
        off_t data_len = root.size;

        // Valid; folders based in root; first.
        char first_path[] = "/mkdirtest1";
        int rc = newfs_mkdir(first_path, mode);
        resolve_path(&root, "/"); // Get updated root object.
        ck_assert_msg(rc == 0, "Wrong error code returned after first folder creation.");
        ck_assert_msg(data_len + KEY_SIZE == root.size,
                        "Size of root has not changed after adding folder.");
        rc = resolve_path(&tmp_dir, first_path);
        ck_assert_msg(rc == 0, "First folder could not be found.");
        char name[tmp_dir.name_len]; get_name(&tmp_dir, name);
        char path[tmp_dir.path_len]; get_path(&tmp_dir, path);
        ck_assert_msg(strcmp(name, "mkdirtest1") == 0, "Name is not set correctly.");
        ck_assert_msg(strcmp(path, first_path) == 0, "Path is not set correctly.");


        // Valid; folders based on root; second.
        data_len = root.size;
        char second_path[] = "/mkdirtest2";
        rc = newfs_mkdir(second_path, mode);
        resolve_path(&root, "/"); // Get updated root object.
        ck_assert_msg(rc == 0, "Wrong error code returned after second folder creation.");
        ck_assert_msg(data_len + KEY_SIZE == root.size,
                      "Size of root has not changed after adding second folder.");
        rc = resolve_path(&tmp_dir, second_path);
        ck_assert_msg(rc == 0, "Second folder could not be found.");

        // Valid; nested folders; first nested.
        data_len = tmp_dir.size; // tmp_dir now has the FCB of /mkdirtest2
        rc = newfs_mkdir("/mkdirtest2/mkdirtest1", mode);
        resolve_path(&tmp_dir, "/mkdirtest2"); // Get updated mkdirtest2 object.
        ck_assert_msg(rc == 0, "Wrong error code returned after first nested creation.");
        ck_assert_msg(data_len + KEY_SIZE == tmp_dir.size,
                      "Size of root has not changed after first nested folder.");
        rc = resolve_path(&tmp_dir, "/mkdirtest2/mkdirtest1");
        ck_assert_msg(rc == 0, "First nested folder could not be found.");

        // Valid; nested folders; second nested.
        rc = resolve_path(&tmp_dir, "/mkdirtest2");
        data_len = tmp_dir.size; // tmp_dir now has the FCB of /mkdirtest2
        rc = newfs_mkdir("/mkdirtest2/mkdirtest2", mode);
        resolve_path(&tmp_dir, "/mkdirtest2"); // Get updated mkdirtest2 object.
        ck_assert_msg(rc == 0, "Wrong error code returned after second nested creation.");
        ck_assert_msg(data_len + KEY_SIZE == tmp_dir.size,
                      "Size of root has not changed after second nested folder.");
        rc = resolve_path(&tmp_dir, "/mkdirtest2/mkdirtest2");
        ck_assert_msg(rc == 0, "Second nested folder could not be found.");


        // Invalid; test exists; root.
        rc = newfs_mkdir("/mkdirtest2", mode);
        ck_assert_msg(rc == -EEXIST, "Error code EEXIST not returned; root.");
        // Invalid; test exists; nested.
        rc = newfs_mkdir("/mkdirtest2/mkdirtest1", mode);
        ck_assert_msg(rc == -EEXIST, "Error code EEXIST not returned; nested.");

        // Invalid; part in path does not exist.
        rc = newfs_mkdir("/mkdirtest3/mkdirtest1", mode);
        ck_assert_msg(rc == -ENOENT, "Error code ENOENT not returned.");

        // Clean-up
        rc = newfs_rmdir("/mkdirtest2/mkdirtest1"); ck_assert_msg(rc == 0, "Cleanup one");
        rc = newfs_rmdir("/mkdirtest2/mkdirtest2"); ck_assert_msg(rc == 0, "Cleanup two");
        rc = newfs_rmdir("/mkdirtest1"); ck_assert_msg(rc == 0, "Cleanup three");
        rc = newfs_rmdir("/mkdirtest2"); ck_assert(rc == 0); ck_assert_msg(rc == 0, "Cleanup four");

        resolve_path(&root, "/");
        printf("%d", (int) root.size);
        ck_assert_msg(root.size == 0, "Root not empty after test.");
    }
END_TEST


START_TEST(check_rmdir)
    {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        // Get root element.
        struct fcb root;
        struct fcb tmp_fcb;
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Root element not empty.");

        // Valid; one folder add and delete.
        int rc = newfs_mkdir("/test", mode);
        resolve_path(&root, "/");
        ck_assert_msg(root.size == KEY_SIZE, "Size did not change after adding a folder.");

        rc = newfs_rmdir("/test");
        resolve_path(&root, "/");
        ck_assert_msg(root.size == 0, "Size did not change after deleting one folder.");
        rc = resolve_path(&tmp_fcb, "/test");
        ck_assert_msg(rc == ENOENT, "Resolving removed dir returned invalid code.");


        // Valid; add several folders.
        newfs_mkdir("/test", mode);
        rc = resolve_path(&tmp_fcb, "/test"); ck_assert_msg(rc == 0, "First not found.");
        newfs_mkdir("/test1", mode);
        rc = resolve_path(&tmp_fcb, "/test1"); ck_assert_msg(rc == 0, "Sec not found.");
        newfs_mkdir("/test2", mode);
        rc = resolve_path(&tmp_fcb, "/test2"); ck_assert_msg(rc == 0, "Th not found.");
        newfs_mkdir("/test2/test1", mode);
        rc = resolve_path(&tmp_fcb, "/test2/test1"); ck_assert_msg(rc == 0, "4th not found.");
        newfs_mkdir("/test2/test2", mode);
        rc = resolve_path(&tmp_fcb, "/test2/test2"); ck_assert_msg(rc == 0, "5 not found.");
        newfs_mkdir("/test1/test1", mode);
        rc = resolve_path(&tmp_fcb, "/test1/test1"); ck_assert_msg(rc == 0, "6 not found.");
        newfs_mkdir("/test1/test2", mode);
        rc = resolve_path(&tmp_fcb, "/test1/test2"); ck_assert_msg(rc == 0, "7 not found.");

        resolve_path(&root, "/");
        ck_assert_msg(root.size == KEY_SIZE * 3, "Size did not change correctly after adding 3 folders.");
        resolve_path(&tmp_fcb, "/test1");
        ck_assert_msg(tmp_fcb.size == 2 * KEY_SIZE, "Size did not change after adding nested folder 1.");
        resolve_path(&tmp_fcb, "/test2");
        ck_assert_msg(tmp_fcb.size == 2 * KEY_SIZE, "Size did not change after adding nested folder 2.");

        // Invalid; remove not empty folder.
        rc = newfs_rmdir("/test1");
        ck_assert_msg(rc == -ENOTEMPTY, "Removing non-empty dir returned wrong result.");

        // Valid; delete nested folders.
        resolve_path(&tmp_fcb, "/test1");
        off_t orig_size = tmp_fcb.size;
        rc = newfs_rmdir("/test1/test1");
        ck_assert_msg(rc == 0, "Removing nested folder returned error. (first)");
        resolve_path(&tmp_fcb, "/test1");
        ck_assert_msg(tmp_fcb.size == orig_size - KEY_SIZE,
                      "Size of parent folder not updated correctly. (first)");

        orig_size = tmp_fcb.size;
        rc = newfs_rmdir("/test1/test2");
        ck_assert_msg(rc == 0, "Removing nested folder returned error. (second)");
        resolve_path(&tmp_fcb, "/test1");
        ck_assert_msg(tmp_fcb.size == orig_size - KEY_SIZE,
                      "Size of parent folder not updated correctly. (second)");

        // Invalid; Remove mount point.
        rc = newfs_rmdir("/");
        ck_assert_msg(rc == -EBUSY, "Removing mount point not resulting in EBUSY code.");

        // Invalid; Remove non-existent folder.
        rc = newfs_rmdir("/r4875y3475");
        ck_assert_msg(rc == -ENOENT, "Removing non-existent dir not resulting in ENOENT code.");

        // Invalid; Remove regular file.
        newfs_create("/testfile", mode, NULL);
        rc = newfs_rmdir("/testfile");
        ck_assert_msg(rc == -ENOTDIR, "Remove file with rmdir wrong error returned.");
    }
END_TEST

// ---- Set up the test suite. ----
Suite *helper_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Newfs");
    tc_core = tcase_create("Helpers");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    // get_fcb.
    tcase_add_test(tc_core, check_get_fcb_root);
    tcase_add_test(tc_core, check_get_fcb_sub_folder);
    // resolve_path.
    tcase_add_test(tc_core, check_resolve_path_root);
    tcase_add_test(tc_core, check_resolve_path_ENOENT);
    tcase_add_test(tc_core, check_resolve_path_sub_folder_from_root);
    // is_dir.
    tcase_add_test(tc_core, check_is_dir);
    // initialize_element.
    tcase_add_test(tc_core, check_initialize_element_common);
    tcase_add_test(tc_core, check_initialize_element_dir);
    tcase_add_test(tc_core, check_initialize_element_file);
    // get_name and set_name.
    tcase_add_test(tc_core, check_set_get_name);
    // get_path and set_path.
    tcase_add_test(tc_core, check_set_get_path);
    // get_data and set_data.
    tcase_add_test(tc_core, check_set_get_data);
    // dat_truncate
    tcase_add_test(tc_core, check_dat_truncate);
    // dat_del_chunk
    tcase_add_test(tc_core, check_dat_del_chunk);
    // dat_insert_chunk
    tcase_add_test(tc_core, check_dat_insert_chunk);
    // tokenize_path
    tcase_add_test(tc_core, check_tokenize_path);
    // separate_path
    tcase_add_test(tc_core, check_separate_path);
    // resolve_ancestor
    tcase_add_test(tc_core, check_resolve_ancestor);
    // get_fcb_from_name
    tcase_add_test(tc_core, check_get_name_from_fcb);
    // remove_UUID_from_dir
    tcase_add_test(tc_core, check_remove_UUID_fr_dir);
    // rm_element_from_directory
    tcase_add_test(tc_core, check_rm_element_fr_dir);


    // Create test-case for fuse functions.
    TCase *tc_fuse;
    tc_fuse = tcase_create("Fuse");
    tcase_add_checked_fixture(tc_fuse, setup, teardown);
    // getattr, chmod, chown.
    tcase_add_test(tc_fuse, check_getattr_chown_chmod);
    // readdir TODO - impl
    // open
    tcase_add_test(tc_fuse, check_open);
    // read and write
    tcase_add_test(tc_fuse, check_read_write);
    // create and unlink
    tcase_add_test(tc_fuse, check_create_and_unlink);
    // utime TODO - impl
    // mkdir
    tcase_add_test(tc_fuse, check_mkdir);
    // rmdir
    tcase_add_test(tc_fuse, check_rmdir);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_fuse);

    return s;
}

int main() {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = helper_suite();
    sr = srunner_create(s);
//    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}